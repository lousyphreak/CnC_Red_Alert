// Minimal stub of UDPAddressClass methods needed by WSMGR for standalone tests.
// The full UDPADDR.CPP drags in FUNCTION.H and the entire game build graph.
#include <string.h>
#include "UDPADDR.H"

UDPAddressClass::UDPAddressClass()
{
	memset(NetworkNumber, 0, 4);
	memset(NodeAddress, 0, 6);
}

UDPAddressClass::UDPAddressClass(NetNumType net, NetNodeType node)
{
	memcpy(NetworkNumber, net, 4);
	memcpy(NodeAddress, node, 6);
}

UDPAddressClass::UDPAddressClass(UDPHeaderType *)
{
	memset(NetworkNumber, 0, 4);
	memset(NodeAddress, 0, 6);
}

void UDPAddressClass::Set_Address(NetNumType net, NetNodeType node)
{
	memcpy(NetworkNumber, net, 4);
	memcpy(NodeAddress, node, 6);
}

void UDPAddressClass::Set_Address(UDPHeaderType *)
{
	memset(NetworkNumber, 1, 4);
	memset(NodeAddress, 1, 6);
}

void UDPAddressClass::Get_Address(NetNumType net, NetNodeType node)
{
	memcpy(net, NetworkNumber, 4);
	memcpy(node, NodeAddress, 6);
}

int UDPAddressClass::Is_Broadcast()
{
	return 0;
}

int UDPAddressClass::operator==(UDPAddressClass &addr)
{
	return (memcmp(NetworkNumber, addr.NetworkNumber, 4) == 0
	        && memcmp(NodeAddress, addr.NodeAddress, 6) == 0);
}

int UDPAddressClass::operator!=(UDPAddressClass &addr)
{
	return !(*this == addr);
}

int UDPAddressClass::operator>(UDPAddressClass &addr)
{
	int c = memcmp(NetworkNumber, addr.NetworkNumber, 4);
	if (c != 0) return c > 0;
	return memcmp(NodeAddress, addr.NodeAddress, 6) > 0;
}

int UDPAddressClass::operator<(UDPAddressClass &addr)
{
	int c = memcmp(NetworkNumber, addr.NetworkNumber, 4);
	if (c != 0) return c < 0;
	return memcmp(NodeAddress, addr.NodeAddress, 6) < 0;
}

int UDPAddressClass::operator<=(UDPAddressClass &addr)
{
	return (*this < addr) || (*this == addr);
}

int UDPAddressClass::operator>=(UDPAddressClass &addr)
{
	return (*this > addr) || (*this == addr);
}
