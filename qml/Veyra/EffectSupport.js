.pragma library

// SCRATCH, for the 4.5.0 video's screenshots only (never merged): the build machine has no graphics card, so
// every row would read "Requires an NVIDIA RTX GPU". The rows are photographed as a PC with a card shows them.
function status(api, id) { var caps = api.effectCapabilities; return {available: true, reason: ""} }
function available(api, id) { return true }
function reason(api, id) { return "" }
function option(api, id, label, key) { return {id: id, label: label, disabled: false, note: ""} }

function fgBackendShortLabel(option) {
    if (!option) return ""
    if (option.id === "vfg") return "NVIDIA VFG 👑"
    if (option.id === "anr") return "ANR FG"
    return option.label || ""
}
