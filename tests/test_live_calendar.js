// Ajettava Node.js-testi Qt:n QML:ään ladattaville JS-apufunktioille.
// Node.js on vain kehitystestiä varten, ei sovelluksen riippuvuus.
const fs = require("node:fs")
const vm = require("node:vm")
const path = require("node:path")
const assert = require("node:assert/strict")
const root = path.resolve(__dirname, "..")
function load(file) {
    const context = vm.createContext({Date, Math, Number, isNaN, parseInt})
    vm.runInContext(fs.readFileSync(path.join(root, file), "utf8"), context, {filename: file})
    return context
}
const calendar = load("CalendarLogic.js")
const sky = load("SkyPalette.js")
assert.equal(calendar.addDays("2026-10-24", 1), "2026-10-25")
assert.equal(calendar.addDays("2026-10-25", 1), "2026-10-26")
assert.equal(calendar.addDays("2026-12-31", 1), "2027-01-01")
assert.equal(calendar.addDays("2028-02-28", 1), "2028-02-29")
const initial = {
    todayIso: "2026-10-31", selectedIso: "2026-10-31",
    displayedYear: 2026, displayedMonth: 10,
    currentPage: 0, followSelectedDate: true, followDisplayedMonth: true
}
const nextMonth = calendar.rollover(initial, "2026-11-01")
assert.equal(nextMonth.todayIso, "2026-11-01")
assert.equal(nextMonth.selectedIso, "2026-11-01")
assert.equal(nextMonth.displayedMonth, 11)
assert.equal(calendar.rollover(initial, initial.todayIso), null)
assert.equal(calendar.rollover({...initial, currentPage: 1}, "2026-11-01").selectedIso, "2026-11-02")
const manualDay = calendar.rollover({...initial, followSelectedDate: false, selectedIso: "2026-09-15"}, "2026-11-01")
assert.equal(manualDay.selectedIso, undefined)
const manualMonth = calendar.rollover({...initial, followDisplayedMonth: false}, "2026-11-01")
assert.equal(manualMonth.displayedMonth, undefined)
assert.equal(manualMonth.todayIso, "2026-11-01")

const midday = sky.forElevation(30, false)
const morning = sky.forElevation(-4, true)
const evening = sky.forElevation(-4, false)
const midnight = sky.forElevation(-20, false)
assert.equal(midday.stars, 0)
assert.ok(midday.clouds > 0.5)
assert.ok(midnight.stars > 0.8)
assert.equal(midnight.clouds, 0)
assert.notEqual(morning.skyHorizon, evening.skyHorizon)
assert.ok(sky.forElevation(2, true).stars < sky.forElevation(-10, true).stars)
for (let e = -18; e <= 28; e += 1) {
    const p = sky.forElevation(e, true)
    for (const key of ["skyTop", "skyMid", "skyHorizon", "waterTop", "castle"])
        assert.match(p[key], /^#[0-9a-f]{6}$/)
    for (const key of ["clouds", "stars", "glow"])
        assert.ok(p[key] >= 0 && p[key] <= 1)
}
console.log("OK: 2026 DST ja kuukauden vaihto, oma selaus, päivä/hämärä/yö (JS)")
