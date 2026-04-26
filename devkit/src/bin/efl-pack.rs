// efl-pack - Headless CLI for the DevKit pack pipeline.
//
// Used by power users and CI smoke jobs that cannot run the egui GUI. It is a
// thin wrapper around `pack_with_request` and `validate_manifest_with_profile`
// in the `efl-devkit` crate, so it stays in lock-step with the GUI build path.
//
// Usage:
//   efl-pack <project_folder> --output <dir> [--profile recommended|strict|legacy] [--validate-only]
//   efl-pack --ci                        # Pull EFL_PACK_PROJECT / EFL_PACK_OUTPUT / etc. from env
//   efl-pack <project_folder> --strict   # Shorthand for --profile strict
//
// Exit codes:
//   0 - success (pack written, or validation clean)
//   1 - validation errors detected
//   2 - usage / I/O error

use std::{path::PathBuf, process::ExitCode};

use efl_devkit::pack::{
    pack_with_request, validate_dat_with_profile, validate_manifest_with_profile, BuildInput,
    BuildRequest, ValidationIssue, ValidationProfile,
};

#[derive(Debug)]
struct CliArgs {
    project: Option<PathBuf>,
    output: Option<PathBuf>,
    profile: ValidationProfile,
    validate_only: bool,
    use_ci_env: bool,
    print_help: bool,
}

impl Default for CliArgs {
    fn default() -> Self {
        Self {
            project: None,
            output: None,
            profile: ValidationProfile::Recommended,
            validate_only: false,
            use_ci_env: false,
            print_help: false,
        }
    }
}

fn parse_args() -> Result<CliArgs, String> {
    let mut args = CliArgs::default();
    let raw: Vec<String> = std::env::args().skip(1).collect();
    let mut i = 0;
    while i < raw.len() {
        let arg = &raw[i];
        match arg.as_str() {
            "-h" | "--help" => args.print_help = true,
            "--ci" => args.use_ci_env = true,
            "--validate-only" => args.validate_only = true,
            "--strict" => args.profile = ValidationProfile::Strict,
            "--legacy" => args.profile = ValidationProfile::Legacy,
            "--profile" => {
                i += 1;
                let value = raw
                    .get(i)
                    .ok_or_else(|| "--profile requires a value".to_string())?;
                args.profile = match value.to_ascii_lowercase().as_str() {
                    "recommended" => ValidationProfile::Recommended,
                    "strict" => ValidationProfile::Strict,
                    "legacy" => ValidationProfile::Legacy,
                    other => return Err(format!("unknown profile \"{other}\"")),
                };
            }
            "--output" | "-o" => {
                i += 1;
                let value = raw
                    .get(i)
                    .ok_or_else(|| "--output requires a path".to_string())?;
                args.output = Some(PathBuf::from(value));
            }
            other if other.starts_with("--") => {
                return Err(format!("unknown flag \"{other}\""));
            }
            _ => {
                if args.project.is_some() {
                    return Err(format!("unexpected positional argument \"{arg}\""));
                }
                args.project = Some(PathBuf::from(arg));
            }
        }
        i += 1;
    }
    Ok(args)
}

fn print_usage() {
    println!(
        "efl-pack {pkg}\n\
\n\
USAGE:\n\
    efl-pack <project_folder> [--output <dir>] [--profile recommended|strict|legacy] [--validate-only]\n\
    efl-pack --ci\n\
\n\
ENV (CI mode):\n\
    EFL_PACK_PROJECT     Path to project folder (mutually exclusive with EFL_PACK_MANIFEST)\n\
    EFL_PACK_MANIFEST    Path to manifest.efl or manifest.efdat\n\
    EFL_PACK_OUTPUT      Output base directory\n\
    EFL_PACK_CONFIG      Optional JSON config file with output_dir / manifest_path / ci_mode\n\
    EFL_PACK_CI          Optional override for ciMode (default true under --ci)\n\
\n\
EXIT:\n\
    0 success | 1 validation errors | 2 usage / I/O error\n",
        pkg = env!("CARGO_PKG_VERSION")
    );
}

fn print_issues(label: &str, issues: &[ValidationIssue]) {
    if issues.is_empty() {
        println!("{label}: clean");
        return;
    }
    println!("{label}:");
    for issue in issues {
        println!(
            "  [{severity}] {code}: {message}",
            severity = issue.severity.to_uppercase(),
            code = issue.code,
            message = issue.message
        );
    }
}

fn classify(issues: &[ValidationIssue]) -> (usize, usize) {
    let mut errors = 0;
    let mut warnings = 0;
    for issue in issues {
        match issue.severity.as_str() {
            "error" => errors += 1,
            "warning" => warnings += 1,
            _ => {}
        }
    }
    (errors, warnings)
}

fn run() -> Result<(), (u8, String)> {
    let args = parse_args().map_err(|e| (2, e))?;
    if args.print_help {
        print_usage();
        return Ok(());
    }

    let request = if args.use_ci_env {
        BuildRequest::from_ci_env().map_err(|e| (2, format!("CI env parse error: {e}")))?
    } else {
        let project = args
            .project
            .clone()
            .ok_or_else(|| (2u8, "missing <project_folder> (or pass --ci)".into()))?;
        BuildRequest {
            input: Some(BuildInput::ProjectFolder(project)),
            output_dir: args.output.clone(),
            config_file: None,
            ci_mode: false,
        }
    };

    let project_dir = match &request.input {
        Some(BuildInput::ProjectFolder(p)) => p.clone(),
        Some(BuildInput::ManifestEntrypoint(m)) => m
            .parent()
            .ok_or_else(|| (2u8, "manifest entrypoint has no parent".into()))?
            .to_path_buf(),
        None => return Err((2, "no input project provided".into())),
    };

    let dat_manifest = project_dir.join("manifest.efdat");
    let efl_manifest = project_dir.join("manifest.efl");

    let (issues, manifest_label) = if dat_manifest.exists() {
        (
            validate_dat_with_profile(&dat_manifest, args.profile)
                .map_err(|e| (2u8, format!("validation error: {e}")))?,
            "manifest.efdat",
        )
    } else if efl_manifest.exists() {
        (
            validate_manifest_with_profile(&efl_manifest, args.profile)
                .map_err(|e| (2u8, format!("validation error: {e}")))?,
            "manifest.efl",
        )
    } else {
        return Err((
            2,
            format!(
                "no manifest.efl or manifest.efdat at {}",
                project_dir.display()
            ),
        ));
    };

    println!("EFL pack pipeline (profile={})", args.profile.id());
    println!("  project: {}", project_dir.display());
    print_issues(&format!("validation [{manifest_label}]"), &issues);
    let (error_count, warn_count) = classify(&issues);

    if error_count > 0 {
        return Err((
            1,
            format!("{error_count} validation error(s), {warn_count} warning(s)"),
        ));
    }

    if args.validate_only {
        println!("validate-only: OK ({warn_count} warning(s))");
        return Ok(());
    }

    if request.output_dir.is_none() {
        return Err((
            2,
            "missing --output <dir> (or set EFL_PACK_OUTPUT under --ci)".into(),
        ));
    }

    let result = pack_with_request(&request).map_err(|e| (2u8, format!("pack failed: {e}")))?;

    println!(
        "build summary  : {summary}",
        summary = result.build_summary
    );
    println!(
        "artifact       : {path}",
        path = result.out_path.display()
    );
    println!("modId          : {}", result.mod_id);
    println!("version        : {}", result.version);
    println!("manifestHash   : {}", result.manifest_hash);
    println!("file checksums : {}", result.file_checksums.len());
    if !result.asset_issues.is_empty() {
        println!("asset notes:");
        for note in &result.asset_issues {
            println!("  - {note}");
        }
    }
    println!("ok");
    Ok(())
}

fn main() -> ExitCode {
    match run() {
        Ok(()) => ExitCode::SUCCESS,
        Err((code, message)) => {
            eprintln!("efl-pack: {message}");
            ExitCode::from(code)
        }
    }
}
