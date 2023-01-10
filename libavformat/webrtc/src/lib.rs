use std::{os::raw::c_char, slice};

use anyhow::Result;
use log::{error, info};
use output::OutputStream;
use tokio::runtime::Runtime;

mod obs_log;
mod output;
mod whip;

pub struct OBSWebRTCOutput {
    stream: OutputStream,
    runtime: Runtime,
}

#[no_mangle]
pub extern "C" fn obs_webrtc_output_new() -> *mut OBSWebRTCOutput {
    (|| -> Result<*mut OBSWebRTCOutput> {
        let runtime = tokio::runtime::Runtime::new()?;
        let stream = runtime.block_on(async { OutputStream::new().await })?;
        Ok(Box::into_raw(Box::new(OBSWebRTCOutput { stream, runtime })))
    })()
    .unwrap_or_else(|e| {
        error!("Unable to create webrtc output: {e:?}");
        std::ptr::null_mut::<OBSWebRTCOutput>()
    })
}

///
/// # Safety
/// Called only from C
#[no_mangle]
pub unsafe extern "C" fn obs_webrtc_output_free(output: *mut OBSWebRTCOutput) {
    info!("Freeing webrtc output");
    if !output.is_null() {
        drop(Box::from_raw(output));
    }
}

///
/// # Safety
/// Called only from C
#[no_mangle]
pub unsafe extern "C" fn obs_webrtc_output_bytes_sent(output: &'static OBSWebRTCOutput) -> u64 {
    output.stream.bytes_sent()
}

///
/// # Safety
/// Called only from C
#[no_mangle]
pub unsafe extern "C" fn obs_webrtc_output_connect(
    output: &'static OBSWebRTCOutput,
    url: *const c_char,
    stream_key: *const c_char,
) {
    let url = std::ffi::CStr::from_ptr(url).to_str().unwrap().to_owned();
    let stream_key = std::ffi::CStr::from_ptr(stream_key)
        .to_str()
        .unwrap()
        .to_owned();

    output.runtime.spawn(async move {
        let result = output.stream.connect(&url, &stream_key).await;
        if let Err(e) = result {
            error!("Failed connecting to webrtc output: {e:?}");
            // Close the peer connection so that future writes fail and disconnect the output
            // TODO: There should be some nuance about a connection failure and a mid-connection failure
            output
                .stream
                .close()
                .await
                .unwrap_or_else(|e| error!("Failed closing webrtc output after error: {e:?}"));
        }
    });
}

#[no_mangle]
pub extern "C" fn obs_webrtc_output_close(output: &'static OBSWebRTCOutput) {
    info!("Closing webrtc output");
    output
        .runtime
        .block_on(async { output.stream.close().await })
        .unwrap_or_else(|e| error!("Failed closing webrtc output: {e:?}"))
}

///
/// # Safety
/// Called only from C
#[no_mangle]
pub unsafe extern "C" fn obs_webrtc_output_write(
    output: &'static OBSWebRTCOutput,
    data: *const u8,
    size: usize,
    duration: u64,
    is_audio: bool,
) -> bool {
    let slice: &[u8] = slice::from_raw_parts(data, size);
    output
        .runtime
        .block_on(async {
            if is_audio {
                output
                    .stream
                    .write_audio(slice, duration)
                    .await
                    .map(|_| true)
            } else {
                output
                    .stream
                    .write_video(slice, duration)
                    .await
                    .map(|_| true)
            }
        })
        .unwrap_or_else(|e| {
            error!("Failed to write packets to webrtc output: {e:?}");
            false
        })
}