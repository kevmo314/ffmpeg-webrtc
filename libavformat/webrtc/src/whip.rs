use reqwest::header::{HeaderValue, AUTHORIZATION};
use webrtc::peer_connection::sdp::session_description::RTCSessionDescription;
use anyhow::Result;


pub async fn offer(
    url: &str,
    stream_key: &str,
    local_desc: RTCSessionDescription,
) -> Result<RTCSessionDescription> {
    let client = reqwest::Client::new();


    let mut headers = reqwest::header::HeaderMap::new();


    headers.append(
        AUTHORIZATION,
        HeaderValue::from_str(&format!("Bearer {stream_key}"))?,
    );


    let res = client
        .post(url)
        .headers(headers)
        .body(local_desc.sdp)
        .send()
        .await?;


    let body = res.text().await?;
    let sdp = RTCSessionDescription::answer(body)?;
    Ok(sdp)
}