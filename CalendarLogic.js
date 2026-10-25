// Aurinko Desktop v0.8: selaimesta ja Qt:sta riippumaton kalenterilogiikka.
// Sijainnin paikallisen päivän määrittää aina C++/QTimeZone, ei JS:n kello.
function addDays(iso, days) {
    const date = new Date(iso + "T12:00:00Z")
    if (isNaN(date.getTime())) return iso
    date.setUTCDate(date.getUTCDate() + days)
    return date.toISOString().slice(0, 10)
}

// Palauttaa vain muuttuvat kentät. Selaus jätetään rauhaan, jos käyttäjä
// on valinnut päivän tai kuukauden itse. Puuttuva tulos = päivä ei muuttunut.
function rollover(state, newToday) {
    if (!newToday || state.todayIso === newToday) return null
    const changes = { todayIso: newToday }
    if (state.followSelectedDate)
        changes.selectedIso = state.currentPage === 1 ? addDays(newToday, 1) : newToday
    if (state.followDisplayedMonth) {
        changes.displayedYear = Number(newToday.slice(0, 4))
        changes.displayedMonth = Number(newToday.slice(5, 7))
    }
    return changes
}
