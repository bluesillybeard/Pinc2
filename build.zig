// Although  we support Zig's build system for compiling the library, we DO NOT have an official binding.
// Pinc's header is so stupidly simple that creating a binding would be extremely trivial - only a single basic source file.
// Feel free to contribute such a thing to this repository!

// TODO: have some decent way to the user to @cImport the header at least - right now, this is only able to build the library

const std = @import("std");
pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const shared = b.option(bool, "shared", "Shared library") orelse false;

    const link_libc = switch (target.result.os.tag) {
        // windows -> does not need libc
        .windows => false,
        // other -> assume libc is required, the C sources will pick up if it's an unsupported platform
        else => true,
    };

    const lib_mod = b.addModule("pinc", .{
        .target = target,
        .optimize = optimize,
        .link_libc = link_libc,
        .pic = shared,
    });

    lib_mod.addCSourceFiles(.{
        .files = &[_][]const u8 {
            // Actual source files
            "src/pinc_main.c",
            "src/platform/platform.c",
            "src/sdl2.c",
            "src/arena.c",
        },
    });

    lib_mod.addIncludePath(b.path("include"));
    lib_mod.addIncludePath(b.path("ext"));
    lib_mod.addIncludePath(b.path("src"));

    const lib = b.addLibrary(.{
        .linkage = if(shared) .dynamic else .static,
        .name = "pinc",
        .root_module = lib_mod,
        // VERSION
        .version = std.SemanticVersion{.major = 2, .minor = 0, .patch = 0},
    });

    lib.addIncludePath(b.path("include"));

    b.installArtifact(lib);
}
