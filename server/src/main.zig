const std = @import("std");
const Server = @import("server.zig").Server;

const Args = struct {
    host: []const u8 = "0.0.0.0",
    port: u16 = 8070,
    gamedata: ?[]const u8 = null,
    emscripten: ?[]const u8 = null,
};

fn parseArgs(allocator: std.mem.Allocator) !Args {
    var args = Args{};
    var it = try std.process.argsWithAllocator(allocator);
    defer it.deinit();
    _ = it.next(); // argv[0]
    while (it.next()) |raw| {
        if (std.mem.eql(u8, raw, "--host")) {
            args.host = try allocator.dupe(u8, it.next() orelse return error.MissingValue);
        } else if (std.mem.eql(u8, raw, "--port")) {
            const v = it.next() orelse return error.MissingValue;
            args.port = try std.fmt.parseInt(u16, v, 10);
        } else if (std.mem.eql(u8, raw, "--gamedata")) {
            args.gamedata = try allocator.dupe(u8, it.next() orelse return error.MissingValue);
        } else if (std.mem.eql(u8, raw, "--emscripten-dir") or std.mem.eql(u8, raw, "--webroot")) {
            args.emscripten = try allocator.dupe(u8, it.next() orelse return error.MissingValue);
        } else if (std.mem.eql(u8, raw, "--help") or std.mem.eql(u8, raw, "-h")) {
            try std.fs.File.stdout().writeAll(
                \\ra-wol-server - Red Alert WOL replacement server
                \\
                \\Usage:
                \\  ra-wol-server [--host HOST] [--port PORT]
                \\                [--gamedata PATH] [--emscripten-dir PATH]
                \\
                \\Endpoints:
                \\  http://HOST:PORT/             static files from --emscripten-dir
                \\  http://HOST:PORT/gamedata/... static files from --gamedata
                \\  ws://HOST:PORT/ws             WOL control plane + relay
                \\
            );
            std.process.exit(0);
        } else {
            std.debug.print("unknown arg: {s}\n", .{raw});
            std.process.exit(2);
        }
    }
    return args;
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var arg_arena = std.heap.ArenaAllocator.init(allocator);
    defer arg_arena.deinit();
    const args = try parseArgs(arg_arena.allocator());

    // Ignore SIGPIPE so writes to a closed socket return EPIPE instead of
    // killing the process.
    const posix = std.posix;
    var sa = posix.Sigaction{
        .handler = .{ .handler = posix.SIG.IGN },
        .mask = posix.sigemptyset(),
        .flags = 0,
    };
    posix.sigaction(posix.SIG.PIPE, &sa, null);

    var srv = try Server.init(allocator, .{
        .host = args.host,
        .port = args.port,
        .gamedata_dir = args.gamedata,
        .emscripten_dir = args.emscripten,
    });
    defer srv.deinit();
    try srv.run();
}
