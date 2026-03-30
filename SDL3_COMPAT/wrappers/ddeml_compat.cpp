#include "ddeml.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct DdeStringHandle {
    explicit DdeStringHandle(std::string value) : value(std::move(value)) {}
    std::string value;
};

struct DdeConversationHandle {
    DdeConversationHandle(std::string service_name, std::string topic_name)
        : service_name(std::move(service_name)), topic_name(std::move(topic_name)) {}
    std::string service_name;
    std::string topic_name;
};

struct DdeDataHandle {
    explicit DdeDataHandle(std::vector<BYTE> bytes) : bytes(std::move(bytes)) {}
    std::vector<BYTE> bytes;
};

struct DdeState {
    std::mutex mutex;
    DWORD next_instance_id = 1;
    std::unordered_map<DWORD, PFNCALLBACK> callbacks;
    std::unordered_map<std::string, DWORD> registered_services;
};

DdeState& dde_state()
{
    static DdeState& state = *new DdeState();
    return state;
}

std::string hsz_to_string(HSZ handle)
{
    if (handle == nullptr) {
        return {};
    }
    return static_cast<DdeStringHandle*>(handle)->value;
}

} // namespace

UINT DdeInitialize(LPDWORD instance_id, PFNCALLBACK callback, DWORD, DWORD)
{
    DdeState& state = dde_state();
    std::scoped_lock lock(state.mutex);
    const DWORD next_id = state.next_instance_id++;
    if (instance_id != nullptr) {
        *instance_id = next_id;
    }
    state.callbacks[next_id] = callback;
    return DMLERR_NO_ERROR;
}

HSZ DdeCreateStringHandle(DWORD, LPCSTR value, int)
{
    return new DdeStringHandle(value != nullptr ? value : "");
}

BOOL DdeFreeStringHandle(DWORD, HSZ string_handle)
{
    delete static_cast<DdeStringHandle*>(string_handle);
    return TRUE;
}

BOOL DdeUninitialize(DWORD instance_id)
{
    DdeState& state = dde_state();
    std::scoped_lock lock(state.mutex);
    state.callbacks.erase(instance_id);

    for (auto it = state.registered_services.begin(); it != state.registered_services.end();) {
        if (it->second == instance_id) {
            it = state.registered_services.erase(it);
        } else {
            ++it;
        }
    }

    return TRUE;
}

HDDEDATA DdeNameService(DWORD instance_id, HSZ service_name, HSZ, UINT command)
{
    if (service_name == nullptr) {
        return nullptr;
    }

    DdeState& state = dde_state();
    std::scoped_lock lock(state.mutex);
    const std::string name = hsz_to_string(service_name);

    if ((command & DNS_REGISTER) != 0U) {
        state.registered_services[name] = instance_id;
        return reinterpret_cast<HDDEDATA>(service_name);
    }

    if ((command & DNS_UNREGISTER) != 0U) {
        state.registered_services.erase(name);
    }

    return nullptr;
}

HCONV DdeConnect(DWORD, HSZ service_name, HSZ topic_name, PCONVCONTEXT)
{
    if (service_name == nullptr) {
        return nullptr;
    }

    DdeState& state = dde_state();
    std::scoped_lock lock(state.mutex);
    const std::string service = hsz_to_string(service_name);
    if (state.registered_services.find(service) == state.registered_services.end()) {
        return nullptr;
    }

    return new DdeConversationHandle(service, hsz_to_string(topic_name));
}

BOOL DdeDisconnect(HCONV conversation)
{
    delete static_cast<DdeConversationHandle*>(conversation);
    return TRUE;
}

HDDEDATA DdeClientTransaction(LPBYTE data, DWORD data_size, HCONV conversation, HSZ, UINT, UINT, DWORD, LPDWORD)
{
    if (conversation == nullptr) {
        return nullptr;
    }

    if (data == nullptr || data_size == 0U) {
        return reinterpret_cast<HDDEDATA>(conversation);
    }

    std::vector<BYTE> bytes(data, data + data_size);
    return new DdeDataHandle(std::move(bytes));
}

DWORD DdeQueryString(DWORD, HSZ string_handle, LPSTR buffer, DWORD max_count, int)
{
    const std::string value = hsz_to_string(string_handle);
    if (buffer != nullptr && max_count > 0U) {
        const size_t count = std::min<size_t>(value.size(), max_count - 1U);
        std::memcpy(buffer, value.data(), count);
        buffer[count] = '\0';
    }
    return static_cast<DWORD>(value.size());
}

LPBYTE DdeAccessData(HDDEDATA data_handle, LPDWORD size)
{
    if (data_handle == nullptr) {
        if (size != nullptr) {
            *size = 0;
        }
        return nullptr;
    }

    auto* handle = static_cast<DdeDataHandle*>(data_handle);
    if (size != nullptr) {
        *size = static_cast<DWORD>(handle->bytes.size());
    }
    return handle->bytes.data();
}

BOOL DdeUnaccessData(HDDEDATA)
{
    return TRUE;
}
