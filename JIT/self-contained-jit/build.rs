use std::process::Command;

fn main() {
    let llvm_config = std::env::var("LLVM_CONFIG").unwrap();
    let get_config = |arg: &str| Command::new(&llvm_config).arg(arg).output().unwrap().stdout;
    let cxxflags = get_config("--cxxflags");
    let cxxflags = std::str::from_utf8(&cxxflags).unwrap();
    let cxxflags = shlex::split(cxxflags).unwrap();
    let libdir = get_config("--libdir");
    let libdir = std::str::from_utf8(&libdir).unwrap();
    let libs = get_config("--libs");
    let libs = std::str::from_utf8(&libs).unwrap();
    let libs = shlex::split(libs).unwrap();

    println!("cargo::rerun-if-changed=src/backend.cpp");
    let mut build = cc::Build::new();
    build.cpp(true);
    build.warnings(false);
    build.file("src/backend.cpp");
    for flag in &cxxflags {
        build.flag(flag);
    }
    build.compile("backend");

    println!("cargo:rustc-link-search={}", libdir);
    for lib in &libs {
        println!("cargo:rustc-link-lib={}", lib.strip_prefix("-l").unwrap());
    }
    println!("cargo:rustc-link-lib=z");
    println!("cargo:rustc-link-lib=zstd");
}
