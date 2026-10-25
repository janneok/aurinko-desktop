// Aurinko Desktop v0.8: saman maiseman väripaletti muuttuu korkeuskulman
// mukaan. Tässä ei ole verkkokuvia eikä lisäriippuvuuksia.
function clamp(value, low, high) { return Math.max(low, Math.min(high, value)) }
function channel(hex, index) { return parseInt(hex.substr(1 + index * 2, 2), 16) }
function mixHex(a, b, t) {
    t = clamp(t, 0, 1)
    const values = []
    for (let i = 0; i < 3; ++i)
        values.push(Math.round(channel(a, i) + (channel(b, i) - channel(a, i)) * t))
    return "#" + values.map(v => v.toString(16).padStart(2, "0")).join("")
}
function mix(a, b, t) {
    const result = {}
    for (const key in a)
        result[key] = typeof a[key] === "number"
            ? a[key] + (b[key] - a[key]) * clamp(t, 0, 1)
            : mixHex(a[key], b[key], t)
    return result
}
// Pimeä, aamurusko, iltarusko ja kirkas sininen päiväsää.
const night = {
    skyTop: "#06152a", skyMid: "#142b4a", skyHorizon: "#3e4560",
    hills: "#0b2138", castle: "#0a1b30",
    waterTop: "#142f49", waterBottom: "#081829",
    stars: 0.86, clouds: 0.0, glow: 0.13
}
const dawn = {
    skyTop: "#21426e", skyMid: "#6b88a8", skyHorizon: "#f7b77c",
    hills: "#304b5b", castle: "#23394c",
    waterTop: "#477593", waterBottom: "#132f4b",
    stars: 0.10, clouds: 0.28, glow: 0.32
}
const dusk = {
    skyTop: "#20355a", skyMid: "#78617d", skyHorizon: "#e99973",
    hills: "#263c51", castle: "#1b2e45",
    waterTop: "#385775", waterBottom: "#12283d",
    stars: 0.18, clouds: 0.18, glow: 0.31
}
const day = {
    skyTop: "#1982c7", skyMid: "#62b4e3", skyHorizon: "#c8eafa",
    hills: "#376e75", castle: "#2c5060",
    waterTop: "#5094b4", waterBottom: "#174364",
    stars: 0.0, clouds: 0.57, glow: 0.42
}

function forElevation(elevation, rising) {
    const twilight = rising ? dawn : dusk
    if (!Number.isFinite(elevation)) return night
    if (elevation <= -12) return night
    if (elevation < -6) return mix(night, twilight, (elevation + 12) / 6)
    if (elevation < 8) return mix(twilight, day, (elevation + 6) / 14)
    return day
}
