-- Integration coverage through the real .zt reader/writer (no MIDI hardware
-- required). Invoked by selftest.lua against its disposable scratch song.
local paths = {os.tmpname(), os.tmpname(), os.tmpname()}
local function read(path)
    local f = assert(io.open(path, "rb"))
    local data = f:read("a"); f:close(); return data
end
local function write(path, data)
    local f = assert(io.open(path, "wb")); f:write(data); f:close()
end
local function chunk(id, data) return id .. string.pack("<I4", #data) .. data end
local function chunks(data)
    local result, pos = {}, 1
    while pos <= #data do
        local id, size, payload = string.unpack("<c4I4", data, pos)
        assert(payload + size - 1 <= #data)
        result[#result + 1] = {id, data:sub(payload, payload + size - 1)}
        pos = payload + size
    end
    return result
end
local function find(data, id)
    for _, c in ipairs(chunks(data)) do if c[1] == id then return c[2] end end
end
local ok, err = pcall(function()
    for i = 0, zt.MAX_INSTRUMENTS - 1 do zt.instrument(i).device = 255 end
    zt.instrument(0).name = "Routing integration"
    zt.instrument(0).channel = 7
    assert(zt.save(paths[1], false))
    local base = read(paths[1])
    assert(not find(base, "MIDR"))

    -- Legacy OFF=64 is normalized to 255, never assigned to an output.
    local old = ""
    for _, c in ipairs(chunks(base)) do
        local data = c[2]
        if c[1] == "ZTin" and data:byte(1) == 0 then
            data = data:sub(1, 4) .. string.char(64) .. data:sub(6)
        end
        old = old .. chunk(c[1], data)
    end
    write(paths[2], old)
    assert(zt.load(paths[2]))
    assert(zt.instrument(0).device == 255)
    assert(zt.instrument(0).channel == 7)

    -- An unavailable legacy index survives a save without remaining a live
    -- playback target. A one-port setup may instead use the reported fallback.
    old = ""
    for _, c in ipairs(chunks(base)) do
        local data = c[2]
        if c[1] == "ZTin" and data:byte(1) == 0 then
            data = data:sub(1, 4) .. string.char(254) .. data:sub(6)
        end
        old = old .. chunk(c[1], data)
    end
    write(paths[2], old)
    assert(zt.load(paths[2]))
    assert(zt.instrument(0).device ~= 254)
    if zt.instrument(0).device == 255 then
        assert(zt.save(paths[1], false))
        assert(find(read(paths[1]), "ZTin"):byte(5) == 254, "lost unresolved legacy slot")
    end

    -- Capture an enumerated destination on a new save, if one exists on
    -- this host. This does not send notes or other MIDI messages.
    zt.instrument(0).device = 0
    assert(zt.save(paths[1], false))
    local captured = find(read(paths[1]), "MIDR")
    if captured then
        assert(zt.load(paths[1]))
        assert(zt.instrument(0).device == 0)
    end

    local name, alias = "ZT regression unavailable synth 76328", "ZT regression alias 76328"
    local metadata = string.pack("<I1I2I1I2I2", 1, 1, 0, #name, #alias) .. name .. alias
    local header_size = 8 + string.unpack("<I4", base, 5)
    for _, before in ipairs({false, true}) do
        local fixture = before and
            (base:sub(1, header_size) .. chunk("MIDR", metadata) .. base:sub(header_size + 1)) or
            (base .. chunk("MIDR", metadata))
        write(paths[2], fixture)
        assert(zt.load(paths[2]))
        assert(zt.instrument(0).device == 255)
        assert(zt.instrument(0).channel == 7)
        for _, compressed in ipairs({false, true}) do
            assert(zt.save(paths[3], compressed))
            assert(zt.load(paths[3]))
            assert(zt.instrument(0).device == 255)
            assert(zt.save(paths[1], false))
            assert(find(read(paths[1]), "MIDR") == metadata, "offline save lost destination")
        end
        -- An explicit user reassignment/disable must discard the old identity.
        zt.instrument(0).device = 255
        assert(zt.save(paths[1], false))
        assert(not find(read(paths[1]), "MIDR"), "disable retained stale destination")
    end
    write(paths[2], base .. chunk("MIDR", metadata:sub(1, -2)))
    assert(not zt.load(paths[2]), "truncated routing metadata was accepted")
    assert(zt.load(paths[3])) -- a failed load must not poison the next load
end)
for _, path in ipairs(paths) do os.remove(path) end
assert(ok, err)
return true
