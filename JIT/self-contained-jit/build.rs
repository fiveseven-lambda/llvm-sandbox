use std::process::Command;

fn main() {
    let llvm_config = std::env::var("LLVM_CONFIG").unwrap();
    let cxxflags = shlex::split(
        std::str::from_utf8(
            &Command::new(&llvm_config)
                .arg("--cxxflags")
                .output()
                .unwrap()
                .stdout,
        )
        .unwrap(),
    )
    .unwrap();
    let libdir = String::from_utf8(
        Command::new(&llvm_config)
            .arg("--libdir")
            .output()
            .unwrap()
            .stdout,
    )
    .unwrap();
    let libs = shlex::split(
        std::str::from_utf8(
            &Command::new(&llvm_config)
                .arg("--libs")
                .arg("--system-libs")
                .output()
                .unwrap()
                .stdout,
        )
        .unwrap(),
    )
    .unwrap();

    println!("cargo::rerun-if-changed=src/backend.cpp");
    let mut build = cc::Build::new();
    build.cpp(true);
    build.warnings(false);
    build.file("src/backend.cpp");
    for flag in &cxxflags {
        build.flag(flag);
    }
    build.compile("backend");

    println!("cargo::rustc-link-search={}", libdir);
    for lib in &libs {
        println!("cargo::rustc-link-lib={}", lib.strip_prefix("-l").unwrap());
    }
    println!("cargo::rustc-link-arg=-export-dynamic");
}
