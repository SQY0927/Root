//! Feature ids shared by the feature manager and boot-patch (which also runs on the host).

#![cfg_attr(not(target_os = "android"), allow(dead_code))]

use anyhow::{Result, bail};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u32)]
pub enum FeatureId {
    SuCompat = 0,
    KernelUmount = 1,
    Sulog = 2,
    AdbRoot = 3,
    SelinuxHide = 4,
    // 5 was webview_zygote_umount (v3.3.0), keep it unused so stale configs don't apply
    ShellSuAdbOnly = 6,
    HideKallsyms = 7,
    SpoofUname = 8,
    HideCmdline = 9,
}

impl FeatureId {
    pub const ALL: [Self; 9] = [
        Self::SuCompat,
        Self::KernelUmount,
        Self::Sulog,
        Self::AdbRoot,
        Self::SelinuxHide,
        Self::ShellSuAdbOnly,
        Self::HideKallsyms,
        Self::SpoofUname,
        Self::HideCmdline,
    ];

    pub const fn from_u32(id: u32) -> Option<Self> {
        match id {
            0 => Some(Self::SuCompat),
            1 => Some(Self::KernelUmount),
            2 => Some(Self::Sulog),
            3 => Some(Self::AdbRoot),
            4 => Some(Self::SelinuxHide),
            6 => Some(Self::ShellSuAdbOnly),
            7 => Some(Self::HideKallsyms),
            8 => Some(Self::SpoofUname),
            9 => Some(Self::HideCmdline),
            _ => None,
        }
    }

    pub const fn name(self) -> &'static str {
        match self {
            Self::SuCompat => "su_compat",
            Self::KernelUmount => "kernel_umount",
            Self::Sulog => "sulog",
            Self::AdbRoot => "adb_root",
            Self::SelinuxHide => "selinux_hide",
            Self::ShellSuAdbOnly => "shell_su_adb_only",
            Self::HideKallsyms => "hide_kallsyms",
            Self::SpoofUname => "spoof_uname",
            Self::HideCmdline => "hide_cmdline",
        }
    }

    pub const fn description(self) -> &'static str {
        match self {
            Self::SuCompat => {
                "SU Compatibility Mode - allows authorized apps to gain root via traditional 'su' command"
            }
            Self::KernelUmount => {
                "Kernel Umount - controls whether kernel automatically unmounts modules when not needed"
            }
            Self::Sulog => {
                "SU Log - streams kernel sulog events to userspace and persists them to disk"
            }
            Self::AdbRoot => "ADB Root - Enable adbd root",
            Self::SelinuxHide => {
                "SELinux Hide - sanitize /sys/fs/selinux access results for app UIDs"
            }
            Self::ShellSuAdbOnly => {
                "Shell SU ADB Only - when shell is granted root, additionally require the process to be spawned by adbd or a KernelSU root process (ksu domain)"
            }
            Self::HideKallsyms => "Hide Kallsyms - sanitize /proc/kallsyms reads for unprivileged UIDs",
            Self::SpoofUname => "Spoof Uname - report a fake kernel release through uname()",
            Self::HideCmdline => "Hide Cmdline - sanitize /proc/cmdline reads for unprivileged UIDs",
        }
    }
}

pub fn parse_feature_id(name: &str) -> Result<FeatureId> {
    match name {
        "su_compat" | "0" => Ok(FeatureId::SuCompat),
        "kernel_umount" | "1" => Ok(FeatureId::KernelUmount),
        "sulog" | "2" => Ok(FeatureId::Sulog),
        "adb_root" | "3" => Ok(FeatureId::AdbRoot),
        "selinux_hide" | "4" => Ok(FeatureId::SelinuxHide),
        "shell_su_adb_only" | "6" => Ok(FeatureId::ShellSuAdbOnly),
        "hide_kallsyms" | "7" => Ok(FeatureId::HideKallsyms),
        "spoof_uname" | "8" => Ok(FeatureId::SpoofUname),
        "hide_cmdline" | "9" => Ok(FeatureId::HideCmdline),
        _ => bail!("Unknown feature: {name}"),
    }
}
