//! Integration test: spawns the server as a subprocess and drives it
//! over raw WebSockets to exercise the opcode table.
//!
//! Build with: `zig build test-integration` (see build.zig).

const std = @import("std");
const net = std.net;
const posix = std.posix;

const proto = @import("proto.zig");
const ws = @import("ws.zig");

const PORT: u16 = 19191;

fn findServerExe(allocator: std.mem.Allocator) ![]u8 {
    // Placed at zig-out/bin/ra-wol-server by `b.installArtifact`.
    return try allocator.dupe(u8, "zig-out/bin/ra-wol-server");
}

const Client = struct {
    sock: posix.socket_t,
    rx: std.ArrayList(u8) = .{},
    allocator: std.mem.Allocator,

    fn connect(allocator: std.mem.Allocator, port: u16) !Client {
        const addr = try std.net.Address.parseIp("127.0.0.1", port);
        const fd = try posix.socket(addr.any.family, posix.SOCK.STREAM | posix.SOCK.CLOEXEC, 0);
        try posix.connect(fd, &addr.any, addr.getOsSockLen());
        return .{ .sock = fd, .allocator = allocator };
    }

    fn deinit(self: *Client) void {
        posix.close(self.sock);
        self.rx.deinit(self.allocator);
    }

    fn writeAll(self: *Client, data: []const u8) !void {
        var off: usize = 0;
        while (off < data.len) {
            const n = try posix.write(self.sock, data[off..]);
            if (n == 0) return error.Closed;
            off += n;
        }
    }

    fn handshake(self: *Client) !void {
        const req =
            "GET /ws HTTP/1.1\r\n" ++
            "Host: 127.0.0.1\r\n" ++
            "Upgrade: websocket\r\n" ++
            "Connection: Upgrade\r\n" ++
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" ++
            "Sec-WebSocket-Version: 13\r\n\r\n";
        try self.writeAll(req);
        // read until \r\n\r\n
        while (true) {
            try self.fillOnce();
            if (std.mem.indexOf(u8, self.rx.items, "\r\n\r\n")) |i| {
                const consume = i + 4;
                const rem = self.rx.items.len - consume;
                if (rem > 0) std.mem.copyForwards(u8, self.rx.items[0..rem], self.rx.items[consume..]);
                self.rx.shrinkRetainingCapacity(rem);
                return;
            }
        }
    }

    fn fillOnce(self: *Client) !void {
        var tmp: [4096]u8 = undefined;
        const n = try posix.read(self.sock, &tmp);
        if (n == 0) return error.Closed;
        try self.rx.appendSlice(self.allocator, tmp[0..n]);
    }

    fn sendMasked(self: *Client, opcode: ws.Opcode, payload: []const u8) !void {
        var buf: std.ArrayList(u8) = .{};
        defer buf.deinit(self.allocator);
        var b0: u8 = @intFromEnum(opcode);
        b0 |= 0x80; // FIN
        try buf.append(self.allocator, b0);
        if (payload.len < 126) {
            try buf.append(self.allocator, 0x80 | @as(u8, @intCast(payload.len)));
        } else if (payload.len <= 0xFFFF) {
            try buf.append(self.allocator, 0x80 | 126);
            var len_bytes: [2]u8 = undefined;
            std.mem.writeInt(u16, &len_bytes, @intCast(payload.len), .big);
            try buf.appendSlice(self.allocator, &len_bytes);
        } else {
            try buf.append(self.allocator, 0x80 | 127);
            var len_bytes: [8]u8 = undefined;
            std.mem.writeInt(u64, &len_bytes, payload.len, .big);
            try buf.appendSlice(self.allocator, &len_bytes);
        }
        const mask: [4]u8 = .{ 0x11, 0x22, 0x33, 0x44 };
        try buf.appendSlice(self.allocator, &mask);
        for (payload, 0..) |byte, i| {
            try buf.append(self.allocator, byte ^ mask[i & 3]);
        }
        try self.writeAll(buf.items);
    }

    /// Sends one application frame.
    fn sendApp(self: *Client, opcode: proto.Opcode, payload: []const u8) !void {
        const frame = try proto.buildFrame(self.allocator, opcode, 0, payload);
        defer self.allocator.free(frame);
        try self.sendMasked(.binary, frame);
    }

    /// Receives exactly one app frame. Loops until a complete FIN binary
    /// message is assembled; ignores ping/close.
    fn recvApp(self: *Client) !struct { opcode: u16, payload: []u8 } {
        var msg: std.ArrayList(u8) = .{};
        errdefer msg.deinit(self.allocator);
        while (true) {
            // The server never masks its frames, but our ws.parseFrame
            // requires mask=1. Build a tiny parser here.
            while (true) {
                const f = parseServerFrame(self.rx.items) catch |err| switch (err) {
                    error.Incomplete => {
                        try self.fillOnce();
                        continue;
                    },
                    else => return err,
                };
                // consume bytes
                const consume = f.consumed;
                try msg.appendSlice(self.allocator, f.payload);
                const rem = self.rx.items.len - consume;
                if (rem > 0) std.mem.copyForwards(u8, self.rx.items[0..rem], self.rx.items[consume..]);
                self.rx.shrinkRetainingCapacity(rem);
                if (f.opcode == @intFromEnum(ws.Opcode.ping) or f.opcode == @intFromEnum(ws.Opcode.pong)) {
                    msg.clearRetainingCapacity();
                    continue;
                }
                if (f.fin) {
                    const pf = try proto.parse(msg.items);
                    const dup = try self.allocator.dupe(u8, pf.payload);
                    msg.deinit(self.allocator);
                    return .{ .opcode = pf.opcode, .payload = dup };
                }
                break;
            }
        }
    }
};

const ServerFrame = struct {
    fin: bool,
    opcode: u4,
    payload: []const u8,
    consumed: usize,
};

fn parseServerFrame(buf: []const u8) !ServerFrame {
    if (buf.len < 2) return error.Incomplete;
    const b0 = buf[0];
    const b1 = buf[1];
    const fin = (b0 & 0x80) != 0;
    const opcode: u4 = @truncate(b0 & 0x0F);
    const masked = (b1 & 0x80) != 0;
    if (masked) return error.UnexpectedMask;
    var len7: u64 = b1 & 0x7F;
    var idx: usize = 2;
    if (len7 == 126) {
        if (buf.len < idx + 2) return error.Incomplete;
        len7 = std.mem.readInt(u16, buf[idx..][0..2], .big);
        idx += 2;
    } else if (len7 == 127) {
        if (buf.len < idx + 8) return error.Incomplete;
        len7 = std.mem.readInt(u64, buf[idx..][0..8], .big);
        idx += 8;
    }
    if (buf.len < idx + len7) return error.Incomplete;
    return .{
        .fin = fin,
        .opcode = opcode,
        .payload = buf[idx .. idx + len7],
        .consumed = idx + len7,
    };
}

fn spawnServer(allocator: std.mem.Allocator) !std.process.Child {
    const exe = try findServerExe(allocator);
    defer allocator.free(exe);
    var child = std.process.Child.init(
        &.{ exe, "--port", std.fmt.comptimePrint("{d}", .{PORT}) },
        allocator,
    );
    child.stdin_behavior = .Ignore;
    child.stdout_behavior = .Inherit;
    child.stderr_behavior = .Inherit;
    try child.spawn();
    // give it time to bind
    std.Thread.sleep(200 * std.time.ns_per_ms);
    return child;
}

fn helloPayload(allocator: std.mem.Allocator, nick: []const u8) ![]u8 {
    var pl: std.ArrayList(u8) = .{};
    const w: proto.PayloadWriter = .{ .buf = &pl, .allocator = allocator };
    try w.writeU32(proto.PROTOCOL_VERSION);
    try w.writeStr(nick);
    return try pl.toOwnedSlice(allocator);
}

test "end-to-end: two clients, game relay" {
    const allocator = std.testing.allocator;

    var child = try spawnServer(allocator);
    defer {
        _ = child.kill() catch {};
    }

    // Host connects and logs in.
    var host = try Client.connect(allocator, PORT);
    defer host.deinit();
    try host.handshake();
    const hello_h = try helloPayload(allocator, "HOST");
    defer allocator.free(hello_h);
    try host.sendApp(.hello, hello_h);
    const welc_h = try host.recvApp();
    defer allocator.free(welc_h.payload);
    try std.testing.expectEqual(@as(u16, @intFromEnum(proto.Opcode.welcome)), welc_h.opcode);
    var r = proto.PayloadReader{ .buf = welc_h.payload };
    const host_cid = try r.readU32();
    _ = try r.readU32();

    // Guest connects and logs in.
    var guest = try Client.connect(allocator, PORT);
    defer guest.deinit();
    try guest.handshake();
    const hello_g = try helloPayload(allocator, "GUEST");
    defer allocator.free(hello_g);
    try guest.sendApp(.hello, hello_g);
    const welc_g = try guest.recvApp();
    defer allocator.free(welc_g.payload);
    var rg = proto.PayloadReader{ .buf = welc_g.payload };
    const guest_cid = try rg.readU32();
    _ = try rg.readU32();
    try std.testing.expect(host_cid != guest_cid);

    // Host creates game.
    var pl: std.ArrayList(u8) = .{};
    defer pl.deinit(allocator);
    const w: proto.PayloadWriter = .{ .buf = &pl, .allocator = allocator };
    try w.writeStr("testgame");
    try w.writeU8(4);
    try w.writeU32(0xdeadbeef);
    try host.sendApp(.game_create, pl.items);

    // Host receives game_list_reply (broadcast) and game_members.
    const m1 = try host.recvApp();
    defer allocator.free(m1.payload);
    try std.testing.expectEqual(@as(u16, @intFromEnum(proto.Opcode.game_list_reply)), m1.opcode);

    const l1 = try host.recvApp();
    defer allocator.free(l1.payload);
    try std.testing.expectEqual(@as(u16, @intFromEnum(proto.Opcode.game_members)), l1.opcode);

    // Guest only receives the game_list_reply broadcast.
    const gl = try guest.recvApp();
    defer allocator.free(gl.payload);
    try std.testing.expectEqual(@as(u16, @intFromEnum(proto.Opcode.game_list_reply)), gl.opcode);
    var glr = proto.PayloadReader{ .buf = gl.payload };
    const gcount = try glr.readU16();
    try std.testing.expectEqual(@as(u16, 1), gcount);
    const gid = try glr.readU32();

    // Guest joins.
    var jpl: std.ArrayList(u8) = .{};
    defer jpl.deinit(allocator);
    const jw: proto.PayloadWriter = .{ .buf = &jpl, .allocator = allocator };
    try jw.writeU32(gid);
    try guest.sendApp(.game_join, jpl.items);

    // After guest joins, server broadcasts updated game_list_reply to
    // everyone, then sends game_members (host + guest) to both members.
    const gl2 = try guest.recvApp();
    defer allocator.free(gl2.payload);
    try std.testing.expectEqual(@as(u16, @intFromEnum(proto.Opcode.game_list_reply)), gl2.opcode);
    const gm = try guest.recvApp();
    defer allocator.free(gm.payload);
    try std.testing.expectEqual(@as(u16, @intFromEnum(proto.Opcode.game_members)), gm.opcode);

    const hl = try host.recvApp();
    defer allocator.free(hl.payload);
    try std.testing.expectEqual(@as(u16, @intFromEnum(proto.Opcode.game_list_reply)), hl.opcode);
    const hm = try host.recvApp();
    defer allocator.free(hm.payload);
    try std.testing.expectEqual(@as(u16, @intFromEnum(proto.Opcode.game_members)), hm.opcode);

    // Host relays broadcast → guest receives relay_from with host's cid.
    try host.sendApp(.relay_broadcast, "hello guest");
    const rf = try guest.recvApp();
    defer allocator.free(rf.payload);
    try std.testing.expectEqual(@as(u16, @intFromEnum(proto.Opcode.relay_from)), rf.opcode);
    var rfr = proto.PayloadReader{ .buf = rf.payload };
    const src = try rfr.readU32();
    try std.testing.expectEqual(host_cid, src);
    try std.testing.expectEqualSlices(u8, "hello guest", rfr.readRest());

    // Guest relays private → host.
    var rp: std.ArrayList(u8) = .{};
    defer rp.deinit(allocator);
    const rpw: proto.PayloadWriter = .{ .buf = &rp, .allocator = allocator };
    try rpw.writeU32(host_cid);
    try rpw.writeBytes("direct");
    try guest.sendApp(.relay_private, rp.items);
    const rf2 = try host.recvApp();
    defer allocator.free(rf2.payload);
    try std.testing.expectEqual(@as(u16, @intFromEnum(proto.Opcode.relay_from)), rf2.opcode);
    var rf2r = proto.PayloadReader{ .buf = rf2.payload };
    try std.testing.expectEqual(guest_cid, try rf2r.readU32());
    try std.testing.expectEqualSlices(u8, "direct", rf2r.readRest());

    // Guest leaves; host should be notified and remain with a one-member game.
    try guest.sendApp(.game_leave, &.{});

    const pl_host = try host.recvApp();
    defer allocator.free(pl_host.payload);
    try std.testing.expectEqual(@as(u16, @intFromEnum(proto.Opcode.peer_left)), pl_host.opcode);

    const gm_after = try host.recvApp();
    defer allocator.free(gm_after.payload);
    try std.testing.expectEqual(@as(u16, @intFromEnum(proto.Opcode.game_members)), gm_after.opcode);
    var gmr = proto.PayloadReader{ .buf = gm_after.payload };
    try std.testing.expectEqual(gid, try gmr.readU32());
    try std.testing.expectEqual(host_cid, try gmr.readU32());
    try std.testing.expectEqual(@as(u16, 1), try gmr.readU16());
    try std.testing.expectEqual(host_cid, try gmr.readU32());
    _ = try gmr.readStr(64);

    const gl_host_after = try host.recvApp();
    defer allocator.free(gl_host_after.payload);
    try std.testing.expectEqual(@as(u16, @intFromEnum(proto.Opcode.game_list_reply)), gl_host_after.opcode);

    const gl_guest_after = try guest.recvApp();
    defer allocator.free(gl_guest_after.payload);
    try std.testing.expectEqual(@as(u16, @intFromEnum(proto.Opcode.game_list_reply)), gl_guest_after.opcode);
}
