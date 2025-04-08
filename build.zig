// Although  we support Zig's build system for compiling the library, we DO NOT have an official binding.
// Pinc's header is so stupidly simple that creating a binding would be extremely trivial - only a single basic source file.
// Feel free to contribute such a thing to this repository!

// TODO: have some decent way for the user to @cImport the header at least - right now, this is only able to build the library

const std = @import("std");
pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const shared = b.option(bool, "shared", "Build a shared library. Default: false") orelse false;
    // if these are not set, leave them undefined so the actual C code can determine the defaults
    const have_window_sdl2: ?bool = b.option(bool, "have_window_sdl2", "see settings.md");
    const enable_error_external: ?bool = b.option(bool, "enable_error_external", "see settings.md");
    const enable_error_assert: ?bool = b.option(bool, "enable_error_assert", "see settings.md");
    const enable_error_user: ?bool = b.option(bool, "enable_error_user", "see settings.md");
    const enable_error_sanitize: ?bool = b.option(bool, "enable_error_sanitize", "see settings.md");
    const enable_error_validate: ?bool = b.option(bool, "enable_error_validate", "see settings.md");

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
        .pic = if(shared) true else null,
    });

    var flags = try std.ArrayList([]const u8).initCapacity(b.allocator, 8);

    if(have_window_sdl2) |enable| {
        try flags.append(if(enable) "-DPINC_HAVE_WINDOW_SDL2=ON" else "-DPINC_HAVE_WINDOW_SDL2=OFF");
    }
    if(enable_error_external) |enable| {
        try flags.append(if(enable) "-DPINC_ENABLE_ERROR_EXTERNAL=ON" else "-DPINC_ENABLE_ERROR_EXTERNAL=OFF");
    }
    if(enable_error_assert) |enable| {
        try flags.append(if(enable) "-DPINC_ENABLE_ERROR_ASSERT=ON" else "-DPINC_ENABLE_ERROR_ASSERT=OFF");
    }
    if(enable_error_user) |enable| {
        try flags.append(if(enable) "-DPINC_ENABLE_ERROR_USER=ON" else "-DPINC_ENABLE_ERROR_USER=OFF");
    }
    if(enable_error_sanitize) |enable| {
        try flags.append(if(enable) "-DPINC_ENABLE_ERROR_SANITIZE=ON" else "-DPINC_ENABLE_ERROR_SANITIZE=OFF");
    }
    if(enable_error_validate) |enable| {
        try flags.append(if(enable) "-DPINC_ENABLE_ERROR_VALIDATE=ON" else "-DPINC_ENABLE_ERROR_VALIDATE=OFF");
    }

    lib_mod.addCSourceFiles(.{
        .files = &[_][]const u8 {
            // Actual source files
            "src/pinc_main.c",
            "src/platform/platform.c",
            "src/sdl2.c",
            "src/arena.c",
        },
        .flags = try flags.toOwnedSlice(),
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
