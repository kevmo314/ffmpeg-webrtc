use log::{Level, Log};

#[no_mangle]
pub extern "C" fn obs_webrtc_install_logger() {
    ObsLogger::install();
    log::info!("webrtc plugin preamble initialized")
}

/// cbindgen:ignore
mod obs {
    #[link(name = "libobs")]
    extern "C" {
        pub fn blog(log_level: i32, format: *const std::os::raw::c_char, ...);
    }
}

pub struct ObsLogger {
    trace_enabled: bool,
}

impl ObsLogger {
    pub fn install() {
        log::set_boxed_logger(Box::new(Self {
            trace_enabled: false,
        }))
        .expect("Logger already set");
        log::set_max_level(log::LevelFilter::Trace);
    }
}

impl Log for ObsLogger {
    fn enabled(&self, metadata: &log::Metadata) -> bool {
        match metadata.level() {
            Level::Trace => self.trace_enabled,
            _ => true
        }
    }

    fn log(&self, record: &log::Record) {
        if !self.enabled(record.metadata()) {
            return;
        }

        let level = record.level();
        let log_level = match level {
            Level::Trace => (400, "T"),
            Level::Debug => (400, "D"),
            Level::Info => (300, "I"),
            Level::Warn => (200, "W"),
            Level::Error => (100, "E"),
        };

        unsafe {
            obs::blog(
                log_level.0,
                format!(
                    "[{}] [{}:{}] {}\0",
                    log_level.1,
                    if record.target().is_empty() {
                        record.module_path().unwrap_or("unk")
                    } else {
                        record.target()
                    },
                    record.line().unwrap_or(0),
                    record.args()
                )
                .as_ptr() as *const i8,
            );
        }
    }

    fn flush(&self) {}
}