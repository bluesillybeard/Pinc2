// Although  we support Zig's build system for compiling the library, we DO NOT have an official binding.
// Pinc's header is so stupidly simple that creating a binding would be extremely trivial - only a single basic source file.
// Feel free to contribute such a thing to this repository!

// In order to use in your own project...
// - zig fetch --save url-to-pinc (whether it's a from github or a vendor folder or whatever)
// Then in your build.zig:
//
// const pinc_dep = b.dependency("Pinc2");
// const pinc_lib = pincDep.artifact("pinc");
// your_exe.linkLibrary(pinc_lib);
// your_exe.addSystemIncludePath(pinc_dep.path("include"));

// If you want to vendor Pinc into your codebase rather than use a Zig dependency:
//
// pinc_lib = [a Compile step where you want to put Pinc's sources]
// pinc_lib.addCSourceFiles(.{
//     .language = .c,
//     .root = b.path("where-ever-your-put-the-pinc-repository"),
//     .flags = &[_][]const u8 {
//         // Assume SDL2 window backend
//         "-DPINC_HAVE_WINDOW_SDL2=ON",
//         // External errors are always useful to have
//         "-DPINC_ENABLE_ERROR_EXTERNAL=ON",
//         // Integrate Pinc's safety settings with Zig's own build safety configurations
//         if(optimize != .Debug and optimize != .ReleaseSafe) "-DPINC_ENABLE_ERROR_ASSERT=OFF" else "-DPINC_ENABLE_ERROR_ASSERT=ON",
//         if(optimize != .Debug and optimize != .ReleaseSafe) "-DPINC_ENABLE_ERROR_USER=OFF" else "-DPINC_ENABLE_ERROR_USER=ON",
//         if(optimize != .Debug and optimize != .ReleaseSafe) "-DPINC_ENABLE_ERROR_SANITIZE=OFF" else "-DPINC_ENABLE_ERROR_SANITIZE=ON",
//         // Validation errors are (theoretically) rather slow
//         if(optimize != .Debug) "-DPINC_ENABLE_ERROR_VALIDATE=OFF" else "-DPINC_ENABLE_ERROR_VALIDATE=ON",
//     },
//     .files = &[_][]const u8 {
//         "src/unitybuild.c
//     }
// });
// pinc_lib.addIncludePath(b.path("path-to-pinc/include"));
// pinc_lib.addIncludePath(b.path("path-to-pinc/src));
// pinc_lib.addIncludePath(b.path("path-to-pinc/ext));
// pinc_lib.addIncludePath(b.path("path-to-pinc/MinGW)); // Technically only needed on Windows, but is fine to keep around for all platforms
// your_exe.addSystemIncludePath(b.path("path-to-pinc/include"));
// your_exe.linkLibrary(pinc_lib);

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
    const use_custom_platform_implementation: ?bool = b.option(bool, "use_custom_platform_implementation", "see settings.md");

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
    if(use_custom_platform_implementation) |enable| {
        try flags.append(if(enable) "-DPINC_USE_CUSTOM_PLATFORM_IMPLEMENTATION=ON" else "-PINC_USE_CUSTOM_PLATFORM_IMPLEMENTATION=OFF");
    }

    lib_mod.addCSourceFiles(.{
        .files = &[_][]const u8 {
            // Actual source files
            "src/pinc_main.c",
            "src/platform/pinc_platform.c",
            "src/pinc_sdl2.c",
            "src/pinc_arena.c",
            "src/libs/pinc_string.c"
        },
        .flags = try flags.toOwnedSlice(),
    });

    lib_mod.addIncludePath(b.path("ext"));
    lib_mod.addIncludePath(b.path("src"));
    lib_mod.addIncludePath(b.path("MinGW"));
    lib_mod.addSystemIncludePath(b.path("include"));

    const lib = b.addLibrary(.{
        .linkage = if(shared) .dynamic else .static,
        .name = "pinc",
        .root_module = lib_mod,
        // VERSION
        .version = std.SemanticVersion{.major = 2, .minor = 0, .patch = 0},
    });

    lib.addIncludePath(b.path("ext"));
    lib.addIncludePath(b.path("src"));
    lib.addSystemIncludePath(b.path("include"));

    b.installArtifact(lib);
}
