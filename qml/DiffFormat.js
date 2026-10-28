.pragma library

// Byte counts the way a person reads them: 980 B, 12.4 KB, 3 MB.
function bytes(size) {
    if (size === undefined || size === null || size < 0)
        return ""
    const units = ["B", "KB", "MB", "GB", "TB"]
    let value = size
    let unit = 0
    while (value >= 1024 && unit < units.length - 1) {
        value /= 1024
        ++unit
    }
    return (unit === 0 ? value : value.toFixed(value < 10 ? 1 : 0)) + " " + units[unit]
}
