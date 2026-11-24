use std::env;
use std::ffi::OsString;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::process::Command;

const BOOTSTRAP_SOURCE: &str = "src/lkm_image_bootstrap.S";
const BOOTSTRAP_OBJECT: &str = "lkm_image_bootstrap.o";
const PREPARED_BOOTSTRAP_OBJECT: &str = ".lkm_image_bootstrap.o";

// Version: read from the repository-root Version.txt, which is two lines:
//   1) versionCode (positive integer)
//   2) versionName (non-empty text)
// Both lines are required; anything malformed invalidates the whole file and
// falls back to the fixed values below so builds without a valid file (CI,
// exported source trees) still report a sane version. A UTF-8 BOM and CR line
// endings are stripped before parsing.
const FALLBACK_VERSION_CODE: u32 = 32680;
const FALLBACK_VERSION_NAME: &str = "Alpha1.0.0";

fn read_version_file() -> Option<(u32, String)> {
    let manifest_dir = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR")?);
    let version_file = manifest_dir.parent()?.parent()?.join("Version.txt");
    // Only watch the file when it exists; otherwise cargo would rebuild the
    // script on every invocation.
    if version_file.is_file() {
        println!("cargo:rerun-if-changed={}", version_file.display());
    }
    let contents = match fs::read_to_string(&version_file) {
        Ok(contents) => contents,
        Err(error) => {
            println!(
                "cargo:warning=Version.txt not readable ({error}); falling back to {FALLBACK_VERSION_CODE} / {FALLBACK_VERSION_NAME}"
            );
            return None;
        }
    };
    let contents = contents.strip_prefix('\u{feff}').unwrap_or(&contents);
    let mut lines = contents
        .lines()
        .map(|line| line.trim_end_matches('\r').trim());
    let code_line = lines.next().unwrap_or_default();
    let name_line = lines.next().unwrap_or_default();
    let digits_only =
        !code_line.is_empty() && code_line.bytes().all(|byte| byte.is_ascii_digit());
    let code = digits_only
        .then(|| code_line.parse::<u32>().ok())
        .flatten();
    match code {
        Some(code) if code > 0 && !name_line.is_empty() => Some((code, name_line.to_string())),
        _ => {
            println!(
                "cargo:warning=Version.txt is invalid (line 1 must be a positive integer and line 2 non-empty); falling back to {FALLBACK_VERSION_CODE} / {FALLBACK_VERSION_NAME}"
            );
            None
        }
    }
}

fn get_version() -> (u32, String) {
    read_version_file().unwrap_or((FALLBACK_VERSION_CODE, FALLBACK_VERSION_NAME.to_string()))
}

fn configure_bindgen() {
    // The bindgen::Builder is the main entry point
    // to bindgen, and lets you build up options for
    // the resulting bindings.
    let mut builder = bindgen::Builder::default()
        // The input header we would like to generate
        // bindings for.
        .header("src/ksu_uapi.h")
        .clang_args(["-x", "c++", "-I../../"])
        // Tell cargo to invalidate the built crate whenever any of the
        // included header files changed.
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()));
    if env::var("CARGO_CFG_TARGET_ARCH").as_deref() == Ok("riscv64") {
        // libc does not yet expose Android's RISC-V signal context. Generate
        // it from the target NDK rather than assuming another libc's layout.
        builder = builder.header_contents("ksu_signal_context.h", "#include <sys/ucontext.h>");
    }
    let bindings = builder
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the Result and panic on failure.
        .expect("Unable to generate bindings");

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_path = std::path::PathBuf::from(env::var("OUT_DIR").unwrap());
    // for debug, uncomment below
    // let out_path = std::path::PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}

fn validate_bootstrap_object(path: &Path) -> io::Result<()> {
    let object = fs::read(path)?;
    let valid = object.len() >= 64
        && object.starts_with(b"\x7fELF")
        && object[4] == 2
        && object[5] == 1
        && u16::from_le_bytes([object[16], object[17]]) == 1
        && u16::from_le_bytes([object[18], object[19]]) == 183;
    if valid {
        Ok(())
    } else {
        Err(io::Error::other(
            "bootstrap object must be a little-endian AArch64 ELF64 ET_REL",
        ))
    }
}

fn copy_bootstrap_object(source: &Path, output: &Path) -> io::Result<()> {
    validate_bootstrap_object(source)?;
    fs::copy(source, output)?;
    Ok(())
}

fn ndk_clang() -> Option<PathBuf> {
    let ndk = env::var_os("ANDROID_NDK_HOME")
        .or_else(|| env::var_os("ANDROID_NDK_ROOT"))
        .map(PathBuf::from)?;
    let prebuilt = ndk.join("toolchains/llvm/prebuilt");
    let mut hosts = fs::read_dir(prebuilt)
        .ok()?
        .filter_map(Result::ok)
        .map(|entry| entry.path())
        .collect::<Vec<_>>();
    hosts.sort();
    hosts.into_iter().find_map(|host| {
        ["clang", "clang.exe"]
            .into_iter()
            .map(|name| host.join("bin").join(name))
            .find(|path| path.is_file())
    })
}

fn run_assembler(program: &Path, arguments: &[OsString]) -> Result<(), String> {
    let output = Command::new(program)
        .args(arguments)
        .output()
        .map_err(|error| format!("{}: {error}", program.display()))?;
    if output.status.success() {
        return Ok(());
    }
    let details = if output.stderr.is_empty() {
        &output.stdout
    } else {
        &output.stderr
    };
    Err(format!(
        "{}: {}",
        program.display(),
        String::from_utf8_lossy(details).trim()
    ))
}

fn assemble_bootstrap() {
    println!("cargo:rerun-if-changed={BOOTSTRAP_SOURCE}");
    println!("cargo:rerun-if-env-changed=KSU_LKM_BOOTSTRAP_OBJECT");
    println!("cargo:rerun-if-env-changed=KSU_LKM_BOOTSTRAP_CC");
    println!("cargo:rerun-if-env-changed=ANDROID_NDK_HOME");
    println!("cargo:rerun-if-env-changed=ANDROID_NDK_ROOT");

    let manifest = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap());
    let source = manifest.join(BOOTSTRAP_SOURCE);
    let output = PathBuf::from(env::var_os("OUT_DIR").unwrap()).join(BOOTSTRAP_OBJECT);

    if let Some(prebuilt) = env::var_os("KSU_LKM_BOOTSTRAP_OBJECT") {
        let prebuilt = PathBuf::from(prebuilt);
        copy_bootstrap_object(&prebuilt, &output).unwrap_or_else(|error| {
            panic!(
                "cannot use KSU_LKM_BOOTSTRAP_OBJECT {}: {error}",
                prebuilt.display()
            )
        });
        return;
    }

    // cross builds prepare this file on the host before entering the container.
    let prepared = manifest.join(PREPARED_BOOTSTRAP_OBJECT);
    println!("cargo:rerun-if-changed={}", prepared.display());
    if prepared.is_file() {
        copy_bootstrap_object(&prepared, &output).unwrap_or_else(|error| {
            panic!(
                "cannot use prepared bootstrap object {}: {error}",
                prepared.display()
            )
        });
        return;
    }

    let mut errors = Vec::new();
    let mut drivers = Vec::<PathBuf>::new();
    if let Some(compiler) = env::var_os("KSU_LKM_BOOTSTRAP_CC") {
        drivers.push(PathBuf::from(compiler));
    }
    drivers.push(PathBuf::from("aarch64-linux-gnu-gcc"));
    if let Some(clang) = ndk_clang() {
        drivers.push(clang);
    }
    drivers.push(PathBuf::from("clang"));

    for driver in drivers {
        let mut arguments = Vec::<OsString>::new();
        if driver
            .file_name()
            .and_then(|name| name.to_str())
            .is_some_and(|name| name.contains("clang"))
        {
            arguments.push("--target=aarch64-linux-gnu".into());
        }
        arguments.extend([
            "-c".into(),
            "-nostdlib".into(),
            "-o".into(),
            output.as_os_str().to_owned(),
            source.as_os_str().to_owned(),
        ]);
        match run_assembler(&driver, &arguments) {
            Ok(()) => {
                validate_bootstrap_object(&output)
                    .expect("assembler produced an invalid bootstrap object");
                return;
            }
            Err(error) => errors.push(error),
        }
    }

    let llvm_mc = PathBuf::from("llvm-mc");
    let llvm_arguments = [
        "-triple=aarch64-linux-gnu".into(),
        "-filetype=obj".into(),
        "-o".into(),
        output.as_os_str().to_owned(),
        source.as_os_str().to_owned(),
    ];
    match run_assembler(&llvm_mc, &llvm_arguments) {
        Ok(()) => {
            validate_bootstrap_object(&output)
                .expect("llvm-mc produced an invalid bootstrap object");
        }
        Err(error) => {
            errors.push(error);
            panic!(
                "cannot assemble the AArch64 LKM bootstrap; install an AArch64 GNU compiler, clang, or llvm-mc, or set KSU_LKM_BOOTSTRAP_OBJECT:\n{}",
                errors.join("\n")
            );
        }
    }
}

fn main() {
    assemble_bootstrap();

    let (code, name) = get_version();
    if env::var("KSU_PACKAGE_NAME").is_err() {
        println!("cargo:rustc-env=KSU_PACKAGE_NAME=hanasakikoi.tklsu.moe");
    }
    println!("cargo:rustc-env=VERSION_CODE={code}");
    println!("cargo:rustc-env=VERSION_NAME={name}");

    let target_os = env::var("CARGO_CFG_TARGET_OS").expect("CARGO_CFG_TARGET_OS not set");
    if target_os == "android" {
        configure_bindgen();
    }
}
