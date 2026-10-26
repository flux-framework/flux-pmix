-------------------------------------------------------------------
-- Copyright 2026 Lawrence Livermore National Security, LLC
-- (c.f. AUTHORS, NOTICE.LLNS, COPYING)
--
-- This file is part of the Flux resource manager framework.
-- For details, see https://github.com/flux-framework.
--
-- SPDX-License-Identifier: LGPL-3.0
-------------------------------------------------------------------

-- pmix.lua - Wireshark dissector for the PMIx client/server wire protocol
--
-- Reads a capture written by the flux-pmix shell plugin
-- (-o pmix.wirecapture=PATH).  Each connection is an interface and
-- each read or write by the client is a packet, tagged with the
-- dissector name "pmix", which is how Wireshark finds this.
--
-- The protocol is that of OpenPMIx, which does not document it; the
-- layouts here are from its source, chiefly src/mca/bfrops and
-- src/mca/ptl, and are written up in doc/guide/wire_protocol.rst
-- for OpenPMIx 2.1 through 6.1.
--
-- A packet holds whatever one read() or write() moved, which has no
-- relation to message boundaries, so each connection's bytes are
-- reassembled here, in each direction, and a message is shown in the
-- packet where its last byte arrived.
--
-- This is installed in Wireshark's global Lua plugin directory, where
-- Wireshark and tshark load it at startup.  To use a copy that is not
-- installed, run
--     tshark -X lua_script:pmix.lua -r capture.pcapng

local proto = Proto("pmix", "PMIx wire protocol")

local f_ifid = Field.new("frame.interface_id")

------------------------------------------------------------------------
-- Protocol constants
------------------------------------------------------------------------

local ENC_V3 = 3
local ENC_V41 = 41
local MODE_UNDESCRIBED = 1
local MODE_DESCRIBED = 2

local HDR_SIZE = 16
local TAG_NOTIFY = 0
local TAG_HEARTBEAT = 1
local TAG_IOF = 2
local TAG_DYNAMIC = 100

local RANK_WILDCARD = 0xfffffffe
local RANK_UNDEF = 0xffffffff

local cmd_names = {
    [0] = "job-info", [1] = "abort", [2] = "commit", [3] = "fence",
    [4] = "get", [5] = "finalize", [6] = "publish", [7] = "lookup",
    [8] = "unpublish", [9] = "spawn", [10] = "connect",
    [11] = "disconnect", [12] = "notify", [13] = "register-events",
    [14] = "deregister-events", [15] = "query", [16] = "log",
    [17] = "allocate", [18] = "job-control", [19] = "monitor",
    [20] = "get-credential", [21] = "validate-credential",
    [22] = "iof-pull", [23] = "iof-push", [24] = "group-construct",
    [25] = "group-join", [26] = "group-invite", [27] = "group-leave",
    [28] = "group-destruct", [29] = "iof-deregister",
    [30] = "fabric-register", [31] = "fabric-update",
    [32] = "compute-device-distances", [34] = "resource-block",
    [35] = "session-control", [36] = "request-sysinfo",
    [37] = "resolve-peers", [38] = "resolve-node",
    [39] = "gds-fallback",
}

local T = {
    UNDEF = 0, BOOL = 1, BYTE = 2, STRING = 3, SIZE = 4, PID = 5,
    INT = 6, INT8 = 7, INT16 = 8, INT32 = 9, INT64 = 10, UINT = 11,
    UINT8 = 12, UINT16 = 13, UINT32 = 14, UINT64 = 15, FLOAT = 16,
    DOUBLE = 17, TIMEVAL = 18, TIME = 19, STATUS = 20, VALUE = 21,
    PROC = 22, APP = 23, INFO = 24, PDATA = 25, BUFFER = 26,
    BYTE_OBJECT = 27, KVAL = 28, PERSIST = 30, POINTER = 31, SCOPE = 32,
    DATA_RANGE = 33, COMMAND = 34, INFO_DIRECTIVES = 35, DATA_TYPE = 36,
    PROC_STATE = 37, PROC_INFO = 38, DATA_ARRAY = 39, PROC_RANK = 40,
    QUERY = 41, COMPRESSED_STRING = 42, ALLOC_DIRECTIVE = 43,
    IOF_CHANNEL = 45, ENVAR = 46,
    -- v4 and later.  These share the v4 wire format and add types;
    -- their layouts are from OpenPMIx's bfrop_base_pack.c.
    COORD = 47, REGATTR = 48, REGEX = 49, JOB_STATE = 50,
    LINK_STATE = 51, PROC_CPUSET = 52, GEOMETRY = 53, DEVICE_DIST = 54,
    ENDPOINT = 55, TOPO = 56, DEVTYPE = 57, LOCTYPE = 58,
    COMPRESSED_BYTE_OBJECT = 59, PROC_NSPACE = 60, DATA_BUFFER = 65,
    STOR_MEDIUM = 66, STOR_ACCESS = 67, STOR_PERSIST = 68,
    STOR_ACCESS_TYPE = 69,
    -- v51
    DEVICE = 70, RESBLOCK_DIRECTIVE = 71, RESOURCE_UNIT = 72,
    -- v61
    NODE_PID = 73, REGEX2 = 74, ALLOC_INHERIT = 75,
}
local type_names = {}
for k, v in pairs(T) do type_names[v] = k end
local function type_name(t)
    return type_names[t] or ("type " .. tostring(t))
end

local scope_names = { [1] = "local", [2] = "remote", [3] = "global" }
local range_names = {
    [1] = "rm", [2] = "local", [3] = "namespace", [4] = "session",
    [5] = "global", [6] = "custom", [7] = "proc-local",
}

------------------------------------------------------------------------
-- Fields
------------------------------------------------------------------------

local pf = {
    dir = ProtoField.string("pmix.direction", "Direction"),
    index = ProtoField.int32("pmix.index", "Peer index"),
    tag = ProtoField.uint32("pmix.tag", "Tag"),
    nbytes = ProtoField.uint32("pmix.nbytes", "Payload length"),
    cmd = ProtoField.uint8("pmix.cmd", "Command", base.DEC, cmd_names),
    reply_to = ProtoField.uint8("pmix.reply_to", "Reply to", base.DEC,
                                cmd_names),
    status = ProtoField.int32("pmix.status", "Status"),
    nspace = ProtoField.string("pmix.nspace", "Namespace"),
    rank = ProtoField.uint32("pmix.rank", "Rank"),
    proc = ProtoField.string("pmix.proc", "Process"),
    key = ProtoField.string("pmix.key", "Key"),
    type = ProtoField.uint16("pmix.type", "Type", base.DEC, type_names),
    value = ProtoField.string("pmix.value", "Value"),
    flags = ProtoField.uint32("pmix.info_flags", "Info flags", base.HEX),
    msg = ProtoField.string("pmix.msg", "Message"),
    security = ProtoField.string("pmix.connect.security", "Security"),
    version = ProtoField.string("pmix.connect.version", "Version"),
    encoding = ProtoField.string("pmix.connect.encoding", "Encoding"),
    mode = ProtoField.string("pmix.connect.mode", "Mode"),
    storage = ProtoField.string("pmix.connect.storage", "Storage"),
    credlen = ProtoField.uint32("pmix.connect.credlen", "Credential length"),
}
proto.fields = pf

local ef_stop = ProtoExpert.new("pmix.undecoded", "Decoding stopped",
                                expert.group.UNDECODED, expert.severity.NOTE)
local ef_bad = ProtoExpert.new("pmix.malformed", "Malformed",
                               expert.group.MALFORMED, expert.severity.ERROR)
proto.experts = { ef_stop, ef_bad }

------------------------------------------------------------------------
-- Cursor: a position in a message payload, with the parameters that
-- decoding depends on.  Every get_* consumes one value and advances,
-- or raises an error, which the caller of decode_* catches.
------------------------------------------------------------------------

local Cursor = {}
Cursor.__index = Cursor

local function cursor_new(tvb, off, len, enc, mode)
    return setmetatable({ tvb = tvb, off = off, stop = off + len,
                          enc = enc, mode = mode }, Cursor)
end

function Cursor:remaining() return self.stop - self.off end

function Cursor:fail(why)
    error({ off = self.off, why = why }, 0)
end

function Cursor:take(n)
    if self:remaining() < n then self:fail("short") end
    local off = self.off
    self.off = off + n
    return off
end

-- Integers are plain Lua numbers, so that this runs on the Lua 5.1
-- and 5.2 of older Wireshark as well as 5.3 and later: no bitwise
-- operators, and arithmetic that is exact to 2^53.  A 64-bit value
-- above that is rare on this wire (a count, length, rank, or status
-- never is), and when one is seen it is kept as a Wireshark UInt64
-- so that it prints exactly.
local TWO32 = 4294967296
local TWO53 = 9007199254740992

-- Fixed-width big-endian unsigned, as v3 uses throughout and both
-- encodings use for single bytes.
function Cursor:be(width)
    local off = self:take(width)
    if width == 8 then
        local hi = self.tvb(off, 4):uint()
        local lo = self.tvb(off + 4, 4):uint()
        if hi >= 2097152 then                   -- 2^21: value >= 2^53
            return UInt64(lo, hi), off
        end
        return hi * TWO32 + lo, off
    end
    return self.tvb(off, width):uint(), off
end

-- Varint, as v4 and later encode integers: LEB128 for up to 8 bytes, then
-- a 9th byte holding bits 56..63 verbatim.
function Cursor:varint()
    local start = self.off
    local v = 0
    local mult = 1
    for i = 1, 8 do
        local b = self.tvb(self:take(1), 1):uint()
        v = v + (b % 128) * mult
        if b < 128 then return v, start end
        mult = mult * 128
    end
    local b = self.tvb(self:take(1), 1):uint()
    v = v + b * mult                            -- mult is 2^56
    if v >= TWO53 then
        -- Redo exactly: reread the bytes into a UInt64.
        local u = UInt64(0, 0)
        local shift = 0
        for i = start, start + 7 do
            local byte = self.tvb(i, 1):uint() % 128
            u = u + UInt64(byte, 0):lshift(shift)
            shift = shift + 7
        end
        u = u + UInt64(b, 0):lshift(56)
        return u, start
    end
    return v, start
end

-- ZigZag: 0, 1, 2, 3, 4 -> 0, -1, 1, -2, 2.  Values here are below
-- 2^53, so the arithmetic is exact; a larger one came back as a
-- UInt64 and is left as it is, since a signed value of that size
-- does not occur on this wire.
local function zigzag(u)
    if type(u) ~= "number" then return u end
    local half = math.floor(u / 2)
    if u % 2 == 1 then return -half - 1 end
    return half
end

function Cursor:unsigned(width)
    if width == 1 then return self:be(1) end
    if self.enc == ENC_V3 then return self:be(width) end
    return self:varint()
end

local function sign_extend(u, width)
    if type(u) ~= "number" then return u end
    if width >= 8 then
        if u >= TWO53 then return u end         -- UInt64 already
        return u                                -- a small value; its
                                                -- sign bit is clear
    end
    local range = ({ 256, 65536, 0, 4294967296 })[width]
    if u >= range / 2 then return u - range end
    return u
end

function Cursor:signed(width)
    if width == 1 then
        local u, off = self:be(1)
        return sign_extend(u, 1), off
    end
    if self.enc == ENC_V3 then
        local u, off = self:be(width)
        return sign_extend(u, width), off
    end
    local u, off = self:varint()
    return zigzag(u), off
end

function Cursor:u8() return self:be(1) end
function Cursor:u16() return self:unsigned(2) end
function Cursor:u32() return self:unsigned(4) end
function Cursor:u64() return self:unsigned(8) end
function Cursor:i32() return self:signed(4) end
function Cursor:i64() return self:signed(8) end
function Cursor:i16() return self:signed(2) end
function Cursor:i8() return self:signed(1) end

-- The "system types": v3 writes a type code naming the sender's width
-- for the C type, then the value at that width.  v41 writes a bare
-- varint, except that PMIX_PID keeps its type code.
function Cursor:size()
    if self.enc == ENC_V3 then
        local start = self.off
        local code = self:be(2)
        if code == T.UINT64 then return self:be(8), start end
        if code == T.UINT32 then return self:be(4), start end
        self:fail("bad size type code " .. code)
    end
    return self:varint()
end

function Cursor:pid()
    local start = self.off
    local code = self:unsigned(2)
    if code ~= T.UINT32 and code ~= T.UINT64 then
        self:fail("bad pid type code " .. code)
    end
    local v
    if self.enc == ENC_V3 then
        v = self:be(code == T.UINT64 and 8 or 4)
    else
        v = self:varint()
    end
    return v, start
end

function Cursor:int()
    if self.enc == ENC_V3 then
        local start = self.off
        local code = self:be(2)
        if code == T.INT32 then return sign_extend(self:be(4), 4), start end
        if code == T.INT64 then return sign_extend(self:be(8), 8), start end
        self:fail("bad int type code " .. code)
    end
    return self:i32()
end

function Cursor:uint()
    if self.enc == ENC_V3 then
        local start = self.off
        local code = self:be(2)
        if code == T.UINT32 then return self:be(4), start end
        if code == T.UINT64 then return self:be(8), start end
        self:fail("bad uint type code " .. code)
    end
    return self:u32()
end

-- PMIX_STRING: INT32 length including the NUL, then that many bytes.
-- Length 0 is a NULL string.  Returns the string (nil for NULL) and
-- the offset and length of the whole encoding.
function Cursor:string()
    local start = self.off
    local len = self:i32()
    if len < 0 then self:fail("negative string length") end
    if len == 0 then return nil, start, self.off - start end
    local off = self:take(len)
    local s = self.tvb(off, len):raw()
    local nul = s:find("\0", 1, true)
    if nul then s = s:sub(1, nul - 1) end
    return s, start, self.off - start
end

-- PMIX_BYTE_OBJECT: SIZE length, then the bytes.  Returns offset and
-- length of the bytes themselves, and the start of the encoding.
function Cursor:byte_object()
    local start = self.off
    local len = self:size()
    if len > self:remaining() then self:fail("byte object overruns") end
    local off = self:take(len)
    return off, len, start
end

-- pack(T, n): in described mode the type code of the count (INT32),
-- the count, and the type code of T; otherwise the count alone.
function Cursor:pack()
    local start = self.off
    local t
    if self.mode == MODE_DESCRIBED then
        local code = self:u16()
        if code ~= T.INT32 then self:fail("bad pack count type " .. code) end
    end
    local n = self:i32()
    if n < 0 then self:fail("negative pack count") end
    if self.mode == MODE_DESCRIBED then t = self:u16() end
    return n, t, start
end

------------------------------------------------------------------------
-- Rendering helpers
------------------------------------------------------------------------

local function u64str(v)
    return tostring(v)
end

local function rank_str(r)
    if r == RANK_WILDCARD then return "*" end
    if r == RANK_UNDEF then return "?" end
    return tostring(r)
end

local function proc_str(ns, r)
    return (ns or "(null)") .. "." .. rank_str(r)
end

local function quote(s)
    if s == nil then return "NULL" end
    return '"' .. s .. '"'
end

local function span(c, start)
    return c.tvb(start, c.off - start)
end

------------------------------------------------------------------------
-- Values.  decode_value adds one value of type t at the cursor to
-- the tree and returns a short rendering for the parent's label.
------------------------------------------------------------------------

local decode_value

-- PMIX_INFO: key, flags, type, value.  Used by info arrays and by the
-- elements of a DATA_ARRAY of INFO.
local function decode_info(c, tree)
    local start = c.off
    local key, koff, klen = c:string()
    local flags = c:u32()
    local t = c:u16()
    local item = tree:add(c.tvb(start, 0), "")
    item:add(pf.key, c.tvb(koff, klen), key or ""):set_hidden()
    if flags ~= 0 then
        item:add(pf.flags, c.tvb(start, 0), flags):set_hidden()
    end
    item:add(pf.type, c.tvb(start, 0), t):set_hidden()
    local text = decode_value(c, item, t)
    item:set_len(c.off - start)
    item:set_text(string.format("%s (%s): %s", key or "(null)",
                                type_name(t), text))
    return string.format("%s=%s", key or "(null)", text)
end

local decode_kval

-- The contents of a pmix.pblob:
-- pack(PROC_RANK) then pack(KVAL) per value, in the client's
-- encoding, in a buffer of its own.
local function decode_pblob(c, tree, off, len)
    local inner = cursor_new(c.tvb, off, len, c.enc, c.mode)
    inner:pack()
    local rank, roff = inner:u32()
    tree:add(pf.rank, span(inner, roff), rank):set_hidden()
    local nkeys = 0
    while inner:remaining() > 0 do
        inner:pack()
        decode_kval(inner, tree)
        nkeys = nkeys + 1
    end
    return string.format("rank %s, %d keys", rank_str(rank), nkeys)
end

-- PMIX_KVAL: key, then PMIX_VALUE (type, value).
function decode_kval(c, tree)
    local start = c.off
    local key, koff, klen = c:string()
    local t = c:u16()
    local item = tree:add(c.tvb(start, 0), "")
    item:add(pf.key, c.tvb(koff, klen), key or ""):set_hidden()
    item:add(pf.type, c.tvb(start, 0), t):set_hidden()
    local text
    if key == "pmix.pblob" and t == T.BYTE_OBJECT then
        local off, len = c:byte_object()
        local ok, res = pcall(decode_pblob, c, item, off, len)
        if ok then
            text = res
        else
            text = string.format("<%d bytes, undecodable>", len)
        end
    else
        text = decode_value(c, item, t)
    end
    item:set_len(c.off - start)
    item:set_text(string.format("%s (%s): %s", key or "(null)",
                                type_name(t), text))
    return key, text
end

function decode_value(c, tree, t)
    local start = c.off
    local function leaf(text)
        tree:add(pf.value, span(c, start), text):set_hidden()
        return text
    end
    if t == T.UNDEF then
        return "undef"
    elseif t == T.BOOL then
        return leaf(c:u8() ~= 0 and "true" or "false")
    elseif t == T.BYTE or t == T.UINT8 or t == T.PERSIST or t == T.SCOPE
        or t == T.DATA_RANGE or t == T.COMMAND or t == T.PROC_STATE then
        local v = c:u8()
        if t == T.SCOPE and scope_names[v] then
            return leaf(scope_names[v])
        elseif t == T.COMMAND and cmd_names[v] then
            return leaf(cmd_names[v])
        end
        return leaf(tostring(v))
    elseif t == T.INT8 then
        return leaf(tostring(c:i8()))
    elseif t == T.INT16 then
        return leaf(tostring(c:i16()))
    elseif t == T.INT32 or t == T.STATUS then
        return leaf(tostring(c:i32()))
    elseif t == T.INT64 then
        return leaf(tostring(c:i64()))
    elseif t == T.INT then
        return leaf(tostring(c:int()))
    elseif t == T.UINT then
        return leaf(tostring(c:uint()))
    elseif t == T.UINT16 or t == T.DATA_TYPE then
        local v = c:u16()
        if t == T.DATA_TYPE then return leaf(type_name(v)) end
        return leaf(tostring(v))
    elseif t == T.UINT32 or t == T.INFO_DIRECTIVES then
        return leaf(tostring(c:u32()))
    elseif t == T.PROC_RANK then
        return leaf(rank_str(c:u32()))
    elseif t == T.UINT64 or t == T.TIME then
        return leaf(u64str(c:u64()))
    elseif t == T.SIZE then
        return leaf(u64str(c:size()))
    elseif t == T.PID then
        return leaf(tostring(c:pid()))
    elseif t == T.FLOAT or t == T.DOUBLE then
        local s = c:string()
        return leaf(s or "NULL")
    elseif t == T.STRING then
        local s = c:string()
        return leaf(quote(s))
    elseif t == T.TIMEVAL then
        local sec = c:i64()
        local usec = c:i64()
        return leaf(string.format("%d.%06d", sec, usec))
    elseif t == T.PROC then
        local ns = c:string()
        local r = c:u32()
        local text = proc_str(ns, r)
        tree:add(pf.proc, span(c, start), text):set_hidden()
        return text
    elseif t == T.BYTE_OBJECT then
        local off, len = c:byte_object()
        return string.format("<%d bytes>", len)
    elseif t == T.BUFFER then
        local btype = c:u8()
        local len = c:size()
        if len > c:remaining() then c:fail("buffer overruns") end
        c:take(len)
        return string.format("<buffer %d bytes>", len)
    elseif t == T.VALUE then
        local vt = c:u16()
        tree:add(pf.type, c.tvb(start, 0), vt):set_hidden()
        return decode_value(c, tree, vt)
    elseif t == T.INFO then
        return decode_info(c, tree)
    elseif t == T.KVAL then
        local key, text = decode_kval(c, tree)
        return string.format("%s=%s", key or "(null)", text)
    elseif t == T.ENVAR then
        local name = c:string()
        local value = c:string()
        c:u8()                                  -- separator
        local text = string.format("%s=%s", name or "(null)",
                                   quote(value))
        tree:add(pf.value, span(c, start), text):set_hidden()
        return text
    elseif t == T.PROC_INFO then
        local ns = c:string()
        local r = c:u32()
        local host = c:string()
        local exe = c:string()
        local pid = c:pid()
        local state = c:u8()
        local text = string.format("%s host=%s exe=%s pid=%d state=%d",
                                   proc_str(ns, r), quote(host),
                                   quote(exe), pid, state)
        tree:add(pf.value, span(c, start), text):set_hidden()
        return text
    elseif t == T.PDATA then
        local ns = c:string()
        local r = c:u32()
        local key = c:string()
        local vt = c:u16()
        local item = tree:add(c.tvb(start, 0), "")
        item:add(pf.key, c.tvb(start, 0), key or ""):set_hidden()
        local text = decode_value(c, item, vt)
        item:set_len(c.off - start)
        item:set_text(string.format("%s %s (%s): %s", proc_str(ns, r),
                                    key or "(null)", type_name(vt), text))
        return string.format("%s %s=%s", proc_str(ns, r), key or "(null)",
                             text)
    elseif t == T.APP then
        local cmd = c:string()
        local argc = c:int()
        local argv = {}
        for i = 1, argc do argv[#argv + 1] = quote(c:string()) end
        local envc = c:i32()
        local item = tree:add(c.tvb(start, 0), "")
        for i = 1, envc do
            item:add(c.tvb(c.off, 0), "Env: " .. quote(c:string()))
        end
        local cwd = c:string()
        local maxprocs = c:int()
        local ninfo = c:size()
        if ninfo > 0 then
            local sub = item:add(c.tvb(c.off, 0), "Info")
            for i = 1, ninfo do decode_value(c, sub, T.INFO) end
            sub:append_text(string.format(" (%d)", ninfo))
        end
        item:set_len(c.off - start)
        local text = string.format("%s [%s] cwd=%s maxprocs=%d env=%d info=%d",
                                   quote(cmd), table.concat(argv, " "),
                                   quote(cwd), maxprocs, envc, ninfo)
        item:set_text("App: " .. text)
        return text
    elseif t == T.QUERY then
        -- Lists inside a compound value have no pack framing: the
        -- packer of the element type is called with a count, which
        -- it does not write.
        local nkeys = c:i32()
        local keys = {}
        for i = 1, nkeys do keys[#keys + 1] = quote(c:string()) end
        local nqual = c:size()
        local item = tree:add(c.tvb(start, 0), "")
        for i = 1, nqual do decode_value(c, item, T.INFO) end
        item:set_len(c.off - start)
        local text = string.format("keys=[%s] qualifiers=%d",
                                   table.concat(keys, " "), nqual)
        item:set_text("Query: " .. text)
        return text
    elseif t == T.ALLOC_DIRECTIVE then
        local v = c:u8()
        local names = { [1] = "new", [2] = "extend", [3] = "release",
                        [4] = "reacquire", [5] = "cancel" }
        return leaf(names[v] or tostring(v))
    elseif t == T.IOF_CHANNEL then
        return leaf(string.format("0x%x", c:u16()))
    elseif t == T.COMPRESSED_STRING then
        local off, len = c:byte_object()
        return string.format("<%d bytes, compressed>", len)
    elseif t == T.JOB_STATE or t == T.LINK_STATE
        or t == T.RESBLOCK_DIRECTIVE then
        return leaf(tostring(c:u8()))
    elseif t == T.LOCTYPE or t == T.STOR_ACCESS_TYPE then
        return leaf(tostring(c:u16()))
    elseif t == T.DEVTYPE or t == T.STOR_MEDIUM or t == T.STOR_ACCESS
        or t == T.STOR_PERSIST then
        return leaf(u64str(c:u64()))
    elseif t == T.PROC_NSPACE then
        return leaf(quote(c:string()))
    elseif t == T.PROC_CPUSET then
        -- hwloc bitmap as a list string; NULL when unbound
        return leaf(quote(c:string()))
    elseif t == T.COMPRESSED_BYTE_OBJECT or t == T.DATA_BUFFER then
        local off, len = c:byte_object()
        return string.format("<%d bytes>", len)
    elseif t == T.COORD then
        local view = c:u8()
        local dims = c:size()
        local coords = {}
        for i = 1, dims do coords[#coords + 1] = tostring(c:u32()) end
        return leaf(string.format("view=%d [%s]", view,
                                  table.concat(coords, ",")))
    elseif t == T.GEOMETRY then
        local fabric = c:size()
        local uuid = c:string()
        local osname = c:string()
        local n = c:size()
        local item = tree:add(c.tvb(start, 0), "")
        for i = 1, n do decode_value(c, item, T.COORD) end
        item:set_len(c.off - start)
        local text = string.format("fabric=%s uuid=%s osname=%s coords=%s",
                                   u64str(fabric), quote(uuid),
                                   quote(osname), u64str(n))
        item:set_text(text)
        return text
    elseif t == T.DEVICE_DIST then
        local uuid = c:string()
        local osname = c:string()
        local devtype = c:u64()
        local mindist = c:u16()
        local maxdist = c:u16()
        return leaf(string.format("uuid=%s osname=%s type=%s min=%d max=%d",
                                  quote(uuid), quote(osname),
                                  u64str(devtype), mindist, maxdist))
    elseif t == T.ENDPOINT then
        local uuid = c:string()
        local osname = c:string()
        local len = c:size()
        if len > c:remaining() then c:fail("endpoint overruns") end
        c:take(len)
        return leaf(string.format("uuid=%s osname=%s <%s bytes>",
                                  quote(uuid), quote(osname), u64str(len)))
    elseif t == T.DEVICE then
        local uuid = c:string()
        local osname = c:string()
        local devtype = c:u64()
        return leaf(string.format("uuid=%s osname=%s type=%s",
                                  quote(uuid), quote(osname),
                                  u64str(devtype)))
    elseif t == T.RESOURCE_UNIT then
        local devtype = c:u64()
        local n = c:size()
        return leaf(string.format("type=%s count=%s", u64str(devtype),
                                  u64str(n)))
    elseif t == T.NODE_PID then
        local host = c:string()
        local nodeid = c:u32()
        local pid = c:pid()
        return leaf(string.format("host=%s nodeid=%d pid=%d",
                                  quote(host), nodeid, pid))
    elseif t == T.REGEX2 then
        local rtype = c:string()
        local len = c:size()
        if len > c:remaining() then c:fail("regex overruns") end
        c:take(len)
        return leaf(string.format("type=%s <%s bytes>", quote(rtype),
                                  u64str(len)))
    elseif t == T.ALLOC_INHERIT then
        return leaf(tostring(c:u8()))
    elseif t == T.REGATTR then
        local name = c:string()
        local key = c:string()
        local vt = c:u16()
        local ndesc = c:i32()
        local descs = {}
        for i = 1, ndesc do descs[#descs + 1] = c:string() or "" end
        return leaf(string.format("%s (%s) %s: %s", name or "", key or "",
                                  type_name(vt), table.concat(descs, " ")))
    elseif t == T.REGEX then
        -- A regex that begins with "pmix" is written as a raw
        -- NUL-terminated string by its own component; any other is a
        -- packed PMIX_STRING.
        local rest = c.tvb(c.off, c:remaining()):raw()
        if rest:sub(1, 4) == "pmix" then
            local nul = rest:find("\0", 1, true)
            if not nul then c:fail("unterminated regex") end
            c:take(nul)
            return leaf(quote(rest:sub(1, nul - 1)))
        end
        return leaf(quote(c:string()))
    elseif t == T.TOPO then
        -- An XML string, then three hwloc support structs whose sizes
        -- are hwloc's, so decoding stops after the string unless it
        -- was NULL.
        local xml = c:string()
        if xml == nil then return leaf("NULL") end
        c:fail("hwloc support structs follow the topology XML")
    elseif t == T.DATA_ARRAY then
        local et = c:u16()
        local n = c:size()
        local label = string.format("%s[%s]", type_name(et), u64str(n))
        if n > 0 and et ~= T.UNDEF then
            for i = 1, n do
                decode_value(c, tree, et)
            end
        end
        return label
    else
        c:fail(string.format("cannot decode %s", type_name(t)))
    end
end

-- A count and that many values of one type, as a single pack, which
-- is how an array of T is packed.
local function decode_array(c, tree, n, t, label)
    local start = c.off
    local item = tree:add(c.tvb(start, 0), label)
    local texts = {}
    if n > 0 then
        c:pack()
        for i = 1, n do
            texts[#texts + 1] = decode_value(c, item, t)
        end
    end
    item:set_len(c.off - start)
    item:append_text(string.format(" (%d)", n))
    -- Scalars are on the summary line already; compound values have
    -- children worth seeing.
    if t ~= T.INFO and t ~= T.APP and t ~= T.QUERY and t ~= T.PDATA then
        item:set_hidden()
    end
    return texts
end

------------------------------------------------------------------------
-- Requests
------------------------------------------------------------------------

-- pack(SIZE n) then pack(T, n) if n > 0: the shape of nearly every
-- list in a request.  Returns the element texts.
local function counted(c, tree, t, label)
    c:pack()
    local n = c:size()
    if n == 0 then return {} end
    return decode_array(c, tree, n, t, label)
end

local function info_part(parts, info)
    if #info > 0 then
        parts[#parts + 1] = "{" .. table.concat(info, " ") .. "}"
    end
end

-- Job data of a connect or spawn reply: byte objects, each holding a
-- pack(STRING nspace) and then pack(KVAL) to the end, in the client's
-- encoding.
local function decode_job_data(c, tree)
    local n = 0
    while c:remaining() > 0 do
        local start = c.off
        c:pack()
        local boff, blen = c:byte_object()
        local item = tree:add(c.tvb(start, 0), "")
        local inner = cursor_new(c.tvb, boff, blen, c.enc, c.mode)
        inner:pack()
        local ns = inner:string()
        local nkeys = 0
        while inner:remaining() > 0 do
            inner:pack()
            decode_kval(inner, item)
            nkeys = nkeys + 1
        end
        item:set_len(c.off - start)
        item:set_text(string.format("Job information for %s (%d keys)",
                                    quote(ns), nkeys))
        n = n + 1
    end
    return n
end

-- Keys sent as one pack(STRING) each, as lookup and unpublish do.
local function separate_strings(c, n)
    local keys = {}
    for i = 1, n do
        c:pack()
        keys[#keys + 1] = quote(c:string())
    end
    return keys
end

local function decode_request(c, tree, cmd, from_server)
    local parts = {}
    if cmd == 3 then                                    -- fence
        c:pack()
        local nprocs = c:size()
        local procs = decode_array(c, tree, nprocs, T.PROC, "Processes")
        parts[#parts + 1] = "procs=[" .. table.concat(procs, " ") .. "]"
        c:pack()
        local ninfo = c:size()
        if ninfo > 0 then
            local info = decode_array(c, tree, ninfo, T.INFO, "Info")
            parts[#parts + 1] = "{" .. table.concat(info, " ") .. "}"
        end
    elseif cmd == 4 then                                -- get
        c:pack()
        local ns = c:string()
        c:pack()
        local r = c:u32()
        tree:add(pf.proc, c.tvb(c.off, 0), proc_str(ns, r)):set_hidden()
        parts[#parts + 1] = "proc=" .. proc_str(ns, r)
        c:pack()
        local ninfo = c:size()
        if ninfo > 0 then
            local info = decode_array(c, tree, ninfo, T.INFO, "Info")
            parts[#parts + 1] = "{" .. table.concat(info, " ") .. "}"
        end
        if c:remaining() > 0 then                       -- absent in 2.x
            c:pack()
            local key, off, len = c:string()
            tree:add(pf.key, c.tvb(off, len), key or ""):set_hidden()
            parts[#parts + 1] = "key=" .. quote(key)
        end
    elseif cmd == 1 then                                -- abort
        c:pack()
        local status, off = c:i32()
        tree:add(pf.status, span(c, off), status):set_hidden()
        parts[#parts + 1] = "status=" .. status
        c:pack()
        local msg, moff, mlen = c:string()
        tree:add(pf.value, c.tvb(moff, mlen), msg or ""):set_hidden()
        parts[#parts + 1] = "msg=" .. quote(msg)
        c:pack()
        local nprocs = c:size()
        if nprocs > 0 then
            local procs = decode_array(c, tree, nprocs, T.PROC, "Processes")
            parts[#parts + 1] = "procs=[" .. table.concat(procs, " ") .. "]"
        end
    elseif cmd == 2 then                                -- commit
        while c:remaining() > 0 do
            local start = c.off
            c:pack()
            local scope = c:u8()
            c:pack()
            local btype = c:u8()
            local len = c:size()
            if len > c:remaining() then c:fail("bucket overruns") end
            local item = tree:add(c.tvb(start, 0), "")
            -- The bucket is KVALs in the same encoding.
            local inner = cursor_new(c.tvb, c.off, len, c.enc, c.mode)
            local nkeys = 0
            while inner:remaining() > 0 do
                inner:pack()
                decode_kval(inner, item)
                nkeys = nkeys + 1
            end
            c:take(len)
            item:set_len(c.off - start)
            local name = scope_names[scope] or ("scope " .. scope)
            item:set_text(string.format("%s: %d keys, %d bytes",
                                        name, nkeys, len))
            parts[#parts + 1] = string.format("%s=%d keys", name, nkeys)
        end
    elseif cmd == 13 then                               -- register-events
        c:pack()
        local ncodes = c:size()
        local codes = decode_array(c, tree, ncodes, T.STATUS, "Codes")
        parts[#parts + 1] = "codes=[" .. table.concat(codes, " ") .. "]"
        c:pack()
        local ninfo = c:size()
        if ninfo > 0 then
            local info = decode_array(c, tree, ninfo, T.INFO, "Info")
            parts[#parts + 1] = "{" .. table.concat(info, " ") .. "}"
        end
    elseif cmd == 14 then                               -- deregister-events
        local codes = {}
        while c:remaining() > 0 do
            c:pack()
            local v, off = c:i32()
            tree:add(pf.status, span(c, off), v):set_hidden()
            codes[#codes + 1] = tostring(v)
        end
        parts[#parts + 1] = "codes=[" .. table.concat(codes, " ") .. "]"
    elseif cmd == 18 then                               -- job-control
        c:pack()
        local ntargets = c:size()
        if ntargets > 0 then
            local procs = decode_array(c, tree, ntargets, T.PROC, "Targets")
            parts[#parts + 1] = "targets=[" .. table.concat(procs, " ") .. "]"
        end
        c:pack()
        local ndirs = c:size()
        if ndirs > 0 then
            local info = decode_array(c, tree, ndirs, T.INFO, "Directives")
            parts[#parts + 1] = "{" .. table.concat(info, " ") .. "}"
        end
    elseif cmd == 12 then
        -- A client's notify request carries the event's range; the
        -- server's event message, on tag 0, carries its source
        -- instead.
        c:pack()
        local code, off = c:i32()
        tree:add(pf.status, span(c, off), code):set_hidden()
        parts[#parts + 1] = "code=" .. code
        c:pack()
        if from_server then
            local ns = c:string()
            local r = c:u32()
            tree:add(pf.proc, c.tvb(c.off, 0), proc_str(ns, r)):set_hidden()
            parts[#parts + 1] = "source=" .. proc_str(ns, r)
        else
            local range = c:u8()
            parts[#parts + 1] = "range=" .. (range_names[range] or range)
        end
        c:pack()
        local ninfo = c:size()
        if ninfo > 0 then
            local info = decode_array(c, tree, ninfo, T.INFO, "Info")
            parts[#parts + 1] = "{" .. table.concat(info, " ") .. "}"
        end
    elseif cmd == 6 then                                -- publish
        c:pack()
        parts[#parts + 1] = "uid=" .. c:u32()
        info_part(parts, counted(c, tree, T.INFO, "Published"))
    elseif cmd == 7 or cmd == 8 then                    -- lookup, unpublish
        c:pack()
        parts[#parts + 1] = "uid=" .. c:u32()
        c:pack()
        local nkeys = c:size()
        local keys = separate_strings(c, nkeys)
        parts[#parts + 1] = "keys=[" .. table.concat(keys, " ") .. "]"
        info_part(parts, counted(c, tree, T.INFO, "Info"))
    elseif cmd == 9 then                                -- spawn
        info_part(parts, counted(c, tree, T.INFO, "Job info"))
        local apps = counted(c, tree, T.APP, "Apps")
        parts[#parts + 1] = string.format("apps=%d", #apps)
    elseif cmd == 10 or cmd == 11 then                  -- connect, disconnect
        local procs = counted(c, tree, T.PROC, "Processes")
        parts[#parts + 1] = "procs=[" .. table.concat(procs, " ") .. "]"
        info_part(parts, counted(c, tree, T.INFO, "Info"))
    elseif cmd == 15 then                               -- query
        local queries = counted(c, tree, T.QUERY, "Queries")
        parts[#parts + 1] = table.concat(queries, "; ")
    elseif cmd == 16 then                               -- log
        -- The timestamp is sent to a 3.0 or later server; since a
        -- capture is of such a server, it is always present.
        c:pack()
        parts[#parts + 1] = "time=" .. u64str(c:u64())
        local data = counted(c, tree, T.INFO, "Data")
        parts[#parts + 1] = string.format("data=%d", #data)
        info_part(parts, counted(c, tree, T.INFO, "Directives"))
    elseif cmd == 17 then                               -- allocate
        c:pack()
        parts[#parts + 1] = "directive=" .. decode_value(c, tree,
                                                         T.ALLOC_DIRECTIVE)
        info_part(parts, counted(c, tree, T.INFO, "Info"))
    elseif cmd == 19 then                               -- monitor
        c:pack()
        parts[#parts + 1] = decode_value(c, tree, T.INFO)
        c:pack()
        parts[#parts + 1] = "error=" .. c:i32()
        info_part(parts, counted(c, tree, T.INFO, "Directives"))
    elseif cmd == 20 then                               -- get-credential
        info_part(parts, counted(c, tree, T.INFO, "Info"))
    elseif cmd == 21 then                               -- validate-credential
        c:pack()
        parts[#parts + 1] = "credential=" .. decode_value(c, tree,
                                                          T.BYTE_OBJECT)
        info_part(parts, counted(c, tree, T.INFO, "Directives"))
    elseif cmd == 22 then                               -- iof-pull
        local procs = counted(c, tree, T.PROC, "Processes")
        parts[#parts + 1] = "procs=[" .. table.concat(procs, " ") .. "]"
        info_part(parts, counted(c, tree, T.INFO, "Directives"))
        c:pack()
        parts[#parts + 1] = "channels=" .. decode_value(c, tree,
                                                        T.IOF_CHANNEL)
        c:pack()
        parts[#parts + 1] = "id=" .. u64str(c:size())
    elseif cmd == 23 then                               -- iof-push
        local procs = counted(c, tree, T.PROC, "Targets")
        parts[#parts + 1] = "targets=[" .. table.concat(procs, " ") .. "]"
        info_part(parts, counted(c, tree, T.INFO, "Directives"))
        if c:remaining() > 0 then
            c:pack()
            parts[#parts + 1] = "data=" .. decode_value(c, tree,
                                                        T.BYTE_OBJECT)
        end
    elseif cmd == 29 then                               -- iof-deregister
        info_part(parts, counted(c, tree, T.INFO, "Directives"))
        c:pack()
        parts[#parts + 1] = "id=" .. u64str(c:size())
    elseif cmd == 24 then                               -- group-construct
        c:pack()
        parts[#parts + 1] = "group=" .. quote(c:string())
        local procs = counted(c, tree, T.PROC, "Processes")
        parts[#parts + 1] = "procs=[" .. table.concat(procs, " ") .. "]"
        info_part(parts, counted(c, tree, T.INFO, "Info"))
    elseif cmd == 27 then                               -- group-leave
        c:pack()
        parts[#parts + 1] = "group=" .. quote(c:string())
        info_part(parts, counted(c, tree, T.INFO, "Info"))
    elseif cmd == 28 then                               -- group-destruct
        c:pack()
        parts[#parts + 1] = "group=" .. quote(c:string())
        local procs = counted(c, tree, T.PROC, "Members")
        parts[#parts + 1] = "members=[" .. table.concat(procs, " ") .. "]"
        info_part(parts, counted(c, tree, T.INFO, "Info"))
    elseif cmd == 30 then                               -- fabric-register
        info_part(parts, counted(c, tree, T.INFO, "Directives"))
    elseif cmd == 31 then                               -- fabric-update
        c:pack()
        parts[#parts + 1] = "index=" .. u64str(c:size())
    elseif cmd == 32 then                               -- device distances
        c:pack()
        parts[#parts + 1] = "topo=" .. decode_value(c, tree, T.TOPO)
        c:pack()
        parts[#parts + 1] = "cpuset=" .. decode_value(c, tree,
                                                      T.PROC_CPUSET)
        info_part(parts, counted(c, tree, T.INFO, "Info"))
    elseif cmd == 34 then                               -- resource-block
        c:pack()
        parts[#parts + 1] = "directive=" .. c:u8()
        c:pack()
        parts[#parts + 1] = "block=" .. quote(c:string())
        local units = counted(c, tree, T.RESOURCE_UNIT, "Units")
        parts[#parts + 1] = string.format("units=%d", #units)
        info_part(parts, counted(c, tree, T.INFO, "Info"))
    elseif cmd == 35 then                               -- session-control
        c:pack()
        parts[#parts + 1] = "session=" .. c:u32()
        info_part(parts, counted(c, tree, T.INFO, "Directives"))
    elseif cmd == 37 then                               -- resolve-peers
        c:pack()
        parts[#parts + 1] = "node=" .. quote(c:string())
        c:pack()
        parts[#parts + 1] = "nspace=" .. quote(c:string())
    elseif cmd == 38 then                               -- resolve-node
        c:pack()
        parts[#parts + 1] = "nspace=" .. quote(c:string())
    elseif cmd == 39 then                               -- gds-fallback
        c:pack()
        parts[#parts + 1] = "module=" .. quote(c:string())
    end
    return table.concat(parts, " ")
end

------------------------------------------------------------------------
-- Replies
------------------------------------------------------------------------

local function decode_reply(c, tree, reply_to)
    if reply_to == 0 then                               -- job-info
        c:pack()
        local ns, off, len = c:string()
        tree:add(pf.nspace, c.tvb(off, len), ns or ""):set_hidden()
        local start = c.off
        local item = tree:add(c.tvb(start, 0), "Job information")
        local n = 0
        while c:remaining() > 0 do
            c:pack()
            decode_kval(c, item)
            n = n + 1
        end
        item:set_len(c.off - start)
        item:append_text(string.format(" (%d keys)", n))
        return string.format("nspace=%s %d keys", quote(ns), n)
    end
    c:pack()
    local status, off = c:i32()
    tree:add(pf.status, span(c, off), status):set_hidden()
    local text = "status=" .. status
    if reply_to == 4 and status == 0 then               -- get
        local n = 0
        while c:remaining() > 0 do
            local start = c.off
            c:pack()
            local boff, blen = c:byte_object()
            local item = tree:add(c.tvb(start, 0), "")
            local inner = cursor_new(c.tvb, boff, blen, c.enc, c.mode)
            inner:pack()
            local ns = inner:string()
            local r = inner:u32()
            item:add(pf.proc, c.tvb(boff, 0), proc_str(ns, r)):set_hidden()
            local nkeys = 0
            while inner:remaining() > 0 do
                inner:pack()
                decode_kval(inner, item)
                nkeys = nkeys + 1
            end
            item:set_len(c.off - start)
            item:set_text(string.format("Data for %s (%d keys)",
                                        proc_str(ns, r), nkeys))
            n = n + 1
        end
        text = text .. string.format(" %d procs", n)
    elseif reply_to == 15 or reply_to == 17 or reply_to == 18
        or reply_to == 19 or reply_to == 21 or reply_to == 30
        or reply_to == 31 or reply_to == 34 or reply_to == 35 then
        -- status, SIZE n, INFO[n]: the common results reply.  The
        -- count is sent whatever the status.
        if c:remaining() > 0 then
            local info = counted(c, tree, T.INFO, "Results")
            text = text .. string.format(" results=%d", #info)
        end
    elseif reply_to == 32 then                          -- device distances
        if c:remaining() > 0 then
            local dist = counted(c, tree, T.DEVICE_DIST, "Distances")
            text = text .. string.format(" distances=%d", #dist)
        end
    elseif reply_to == 37 then                          -- resolve-peers
        if c:remaining() > 0 then
            local procs = counted(c, tree, T.PROC, "Processes")
            text = text .. " procs=[" .. table.concat(procs, " ") .. "]"
        end
    elseif reply_to == 7 then                           -- lookup
        if c:remaining() > 0 then
            local data = counted(c, tree, T.PDATA, "Data")
            text = text .. string.format(" %d values", #data)
        end
    elseif reply_to == 38 then                          -- resolve-node
        if c:remaining() > 0 then
            c:pack()
            text = text .. " nodes=" .. quote(c:string())
        end
    elseif reply_to == 9 then                           -- spawn
        if c:remaining() > 0 then
            c:pack()
            text = text .. " nspace=" .. quote(c:string())
        end
        if status == 0 and c:remaining() > 0 then
            local start = c.off
            local item = tree:add(c.tvb(start, 0), "Job information")
            local n = 0
            while c:remaining() > 0 do
                c:pack()
                decode_kval(c, item)
                n = n + 1
            end
            item:set_len(c.off - start)
            item:append_text(string.format(" (%d keys)", n))
            text = text .. string.format(" %d keys", n)
        end
    elseif status ~= 0 or c:remaining() == 0 then
        -- A failed reply, or one with nothing after the status.
    elseif reply_to == 10 then                          -- connect
        text = text .. string.format(" %d namespaces",
                                     decode_job_data(c, tree))
    elseif reply_to == 24 then                          -- group-construct
        local members = counted(c, tree, T.PROC, "Members")
        text = text .. string.format(" members=%d", #members)
        if c:remaining() > 0 then
            -- 6.0 and later send a BOOL before the context id; 4.x
            -- and 5.0 send the id alone.  A BOOL is one byte and a
            -- SIZE varint is at least one, so the form is told by
            -- the client's version, which the summary line has.
            c:pack()
            if c.client_major and c.client_major >= 6 then
                local has = c:u8() ~= 0
                if has then
                    c:pack()
                    text = text .. " ctxid=" .. u64str(c:size())
                end
                if c:remaining() > 0 then
                    local info = counted(c, tree, T.INFO, "Group info")
                    text = text .. string.format(" info=%d", #info)
                end
            else
                text = text .. " ctxid=" .. u64str(c:size())
            end
        end
    elseif reply_to == 20 then                          -- get-credential
        c:pack()
        text = text .. " credential=" .. decode_value(c, tree,
                                                      T.BYTE_OBJECT)
        local info = counted(c, tree, T.INFO, "Info")
        text = text .. string.format(" info=%d", #info)
    elseif reply_to == 22 then                          -- iof-pull
        c:pack()
        text = text .. " id=" .. u64str(c:size())
    end
    return text
end

------------------------------------------------------------------------
-- Connect request: hand-encoded, with no pack framing.
------------------------------------------------------------------------

-- Parse the body into a table of fields, each with its offset and
-- length, so the same parse serves reassembly and rendering.
local function parse_connect(tvb)
    local off = 0
    local hs = {}
    -- Strings are NUL-terminated and may be UTF-8 (a Flux jobid
    -- namespace begins with U+0192), so take the raw bytes.
    local function cstring(name)
        local rest = tvb(off):raw()
        local nul = rest:find("\0", 1, true)
        if not nul then error("unterminated string") end
        local s = rest:sub(1, nul - 1)
        hs[name] = { v = s, off = off, len = nul }
        off = off + nul
        return s
    end
    cstring("security")
    local credlen = tvb(off, 4):uint()
    hs.credlen = { v = credlen, off = off, len = 4 }
    off = off + 4
    hs.cred = { off = off, len = credlen }
    off = off + credlen
    hs.flag = { v = tvb(off, 1):uint(), off = off, len = 1 }
    off = off + 1
    if hs.flag.v ~= 0 then return hs end
    cstring("nspace")
    hs.rank = { v = tvb(off, 4):uint(), off = off, len = 4 }
    off = off + 4
    cstring("version")
    local encoding = cstring("encoding")
    hs.buftype = { v = tvb(off, 1):uint(), off = off, len = 1 }
    off = off + 1
    cstring("storage")
    if off < tvb:len() then
        hs.info = { off = off, len = tvb:len() - off }
    end
    -- v21 and v3 share one wire format, as do v4, v41, v51, and v61;
    -- the later module of each pair only adds types.
    if encoding == "v21" or encoding == "v3" then
        hs.enc = ENC_V3
    elseif encoding == "v4" or encoding == "v41" or encoding == "v51"
        or encoding == "v61" then
        hs.enc = ENC_V41
    end
    if hs.buftype.v == MODE_UNDESCRIBED or hs.buftype.v == MODE_DESCRIBED then
        hs.mode = hs.buftype.v
    end
    return hs
end

local function render_connect_fields(tvb, tree, hs)
    -- The summary line shows these; they are for filtering.
    local function add(field, f, v)
        tree:add(field, tvb(f.off, f.len), v or f.v):set_hidden()
    end
    add(pf.security, hs.security)
    add(pf.credlen, hs.credlen)
    tree:add(tvb(hs.cred.off, hs.cred.len),
             string.format("Credential (%d bytes)", hs.cred.len))
    if hs.flag.v ~= 0 then
        tree:add(tvb(hs.flag.off, 1),
                 "Flag: " .. hs.flag.v .. " (not a simple client)")
        return string.format("flag=%d (not a simple client)", hs.flag.v)
    end
    add(pf.nspace, hs.nspace)
    add(pf.rank, hs.rank)
    add(pf.version, hs.version)
    add(pf.encoding, hs.encoding)
    local mode = hs.buftype.v == MODE_DESCRIBED and "described"
                 or hs.buftype.v == MODE_UNDESCRIBED and "undescribed"
                 or ("buftype " .. hs.buftype.v)
    add(pf.mode, hs.buftype, mode)
    add(pf.storage, hs.storage)
    if hs.info then
        tree:add(tvb(hs.info.off, hs.info.len),
                 string.format("Info (%d bytes)", hs.info.len))
    end
    local text = string.format(
        "%s.%d version=%s security=%s encoding=%s mode=%s storage=%s",
        hs.nspace.v, hs.rank.v, hs.version.v, hs.security.v,
        hs.encoding.v, mode, hs.storage.v)
    if not hs.enc then text = text .. " (encoding unsupported)" end
    if not hs.mode then text = text .. " (mode unsupported)" end
    return text
end

------------------------------------------------------------------------
-- Reassembly.  Done once, on the first pass through the capture, when
-- Wireshark visits packets in file order.  Each complete message is
-- filed under the frame in which it completed, with everything needed
-- to render it on any later visit.
------------------------------------------------------------------------

local conns = {}        -- by interface id
local frames = {}       -- by frame number: list of records

-- Wireshark calls this before each first pass: at load, and again
-- when a file is reloaded or another is opened in the same session.
-- Without it, the records filed from the last file would be rendered
-- for the frame numbers of the next.
function proto.init()
    conns = {}
    frames = {}
end

local function conn_get(ifid)
    local c = conns[ifid]
    if not c then
        c = { buf = { [0] = ByteArray.new(), [1] = ByteArray.new() },
              pending = {}, hs_done = false, resp_done = false,
              broken = false }
        conns[ifid] = c
    end
    return c
end

local function u32be(ba, off)
    return ((ba:get_index(off) * 256 + ba:get_index(off + 1)) * 256
            + ba:get_index(off + 2)) * 256 + ba:get_index(off + 3)
end

local function u32le(ba, off)
    return ((ba:get_index(off + 3) * 256 + ba:get_index(off + 2)) * 256
            + ba:get_index(off + 1)) * 256 + ba:get_index(off)
end

-- ByteArray:subset() rejects an empty range, so the tail of a buffer
-- that was consumed whole is a new empty array.
local function tail(ba, off)
    local n = ba:len() - off
    if n <= 0 then return ByteArray.new() end
    return ba:subset(off, n)
end

local function file(num, rec)
    local list = frames[num]
    if not list then
        list = {}
        frames[num] = list
    end
    list[#list + 1] = rec
end

-- The command of a request, needed at reassembly time to pair the
-- reply.  A pack count and a byte, in either encoding and mode.
local function peek_command(rec)
    local tvb = rec.bytes:tvb("peek")
    local c = cursor_new(tvb, 0, tvb:len(), rec.enc, rec.mode)
    local ok, cmd = pcall(function()
        c:pack()
        return c:u8()
    end)
    if ok then return cmd end
    return nil
end

-- Take as many whole messages as the buffer holds.
local function reassemble(conn, dir, num)
    local buf = conn.buf[dir]
    while true do
        local len = buf:len()
        if dir == 0 and not conn.hs_done then
            -- Connect request: host-order header, then the body.
            if len < HDR_SIZE then return end
            local nbytes = u32le(buf, 8)
            if len < HDR_SIZE + nbytes then return end
            local rec = { kind = "connect", dir = dir,
                          bytes = buf:subset(HDR_SIZE, nbytes),
                          index = u32le(buf, 0), tag = u32le(buf, 4) }
            -- The body says which encoding the rest of the
            -- connection uses, so it is parsed here, not just at
            -- render time.
            local ok, hs = pcall(parse_connect, rec.bytes:tvb("hs"))
            if ok then
                rec.hs = hs
                conn.enc = hs.enc
                conn.mode = hs.mode
                conn.major = hs.version
                             and tonumber(hs.version.v:match("^(%d+)"))
                conn.ns = hs.nspace and hs.nspace.v
                conn.rank = hs.rank and hs.rank.v
                if not hs.enc or not hs.mode then conn.broken = true end
            else
                conn.broken = true
            end
            file(num, rec)
            buf = tail(buf, HDR_SIZE + nbytes)
            conn.buf[dir] = buf
            conn.hs_done = true
            if conn.broken then return end
        elseif dir == 1 and not conn.resp_done then
            -- Connect response: status, then peer index on success.
            if len < 4 then return end
            local status = u32be(buf, 0)
            if status >= 0x80000000 then status = status - 0x100000000 end
            if status ~= 0 then
                file(num, { kind = "response", status = status })
                conn.buf[dir] = tail(buf, 4)
                conn.broken = true
                return
            end
            if len < 8 then return end
            file(num, { kind = "response", status = 0,
                        pindex = u32be(buf, 4) })
            conn.buf[dir] = tail(buf, 8)
            buf = conn.buf[dir]
            conn.resp_done = true
        else
            if len < HDR_SIZE then return end
            local index = u32be(buf, 0)
            local tag = u32be(buf, 4)
            local nbytes = u32be(buf, 8)
            if len < HDR_SIZE + nbytes then return end
            local rec = { kind = "message", dir = dir, index = index,
                          tag = tag, nbytes = nbytes,
                          bytes = buf:subset(HDR_SIZE, nbytes),
                          enc = conn.enc, mode = conn.mode,
                          major = conn.major }
            -- Pair replies with requests by tag.
            if dir == 0 and tag >= TAG_DYNAMIC then
                local cmd = peek_command(rec)
                if cmd then conn.pending[tag] = cmd end
            elseif dir == 1 and tag >= TAG_DYNAMIC then
                rec.reply_to = conn.pending[tag]
                conn.pending[tag] = nil
            end
            file(num, rec)
            buf = tail(buf, HDR_SIZE + nbytes)
            conn.buf[dir] = buf
        end
    end
end

------------------------------------------------------------------------
-- Dissector
------------------------------------------------------------------------

local function render_message(rec, tree, pinfo, conn)
    local tvb = rec.bytes:tvb("PMIx message")
    local arrow = rec.dir == 0 and ">" or "<"
    local item = tree:add(proto, tvb(), "PMIx")
    -- The summary line shows the header; these are for filtering.
    item:add(pf.dir, tvb(0, 0),
             rec.dir == 0 and "to server" or "to client"):set_hidden()
    item:add(pf.index, tvb(0, 0), rec.index):set_hidden()
    item:add(pf.tag, tvb(0, 0), rec.tag):set_hidden()
    item:add(pf.nbytes, tvb(0, 0), rec.nbytes):set_hidden()

    local label
    if rec.tag == TAG_HEARTBEAT then
        label = "heartbeat"
    elseif rec.tag == TAG_IOF then
        label = "iof"
    elseif rec.enc == nil then
        label = "(encoding unknown: handshake not seen)"
    else
        local c = cursor_new(tvb, 0, tvb:len(), rec.enc, rec.mode)
        c.client_major = rec.major
        local ok, err = pcall(function()
            if rec.dir == 0 or rec.tag == TAG_NOTIFY then
                c:pack()
                local cmd, off = c:u8()
                item:add(pf.cmd, span(c, off), cmd):set_hidden()
                local name = cmd_names[cmd] or ("cmd " .. cmd)
                local detail = decode_request(c, item, cmd, rec.dir == 1)
                label = name
                if detail ~= "" then label = label .. " " .. detail end
            else
                local name = rec.reply_to and cmd_names[rec.reply_to]
                if rec.reply_to then
                    item:add(pf.reply_to, tvb(0, 0), rec.reply_to):set_hidden()
                end
                label = name and (name .. "-reply") or "reply"
                label = label .. " " .. decode_reply(c, item, rec.reply_to)
            end
            if c:remaining() > 0 then
                item:add(tvb(c.off), string.format("Trailing: %d bytes",
                                                   c:remaining()))
                label = label .. string.format(" +%d bytes", c:remaining())
            end
        end)
        if not ok then
            local why = type(err) == "table" and err.why or tostring(err)
            local off = type(err) == "table" and err.off or 0
            if off > tvb:len() then off = tvb:len() end
            item:add_proto_expert_info(ef_stop, string.format(
                "undecoded at +%d: %s", off, why))
            if off < tvb:len() then
                item:add(tvb(off), string.format("Undecoded: %d bytes",
                                                 tvb:len() - off))
            end
            label = (label or "?") .. string.format(" <undecoded at +%d: %s>",
                                                    off, why)
        end
    end
    local text = string.format("%s tag=%d len=%d %s", arrow, rec.tag,
                               rec.nbytes, label)
    item:set_text("PMIx " .. text)
    item:add(pf.msg, tvb(0, 0), text):set_hidden()
    return text
end

local function render_connect(rec, tree, pinfo, conn)
    local tvb = rec.bytes:tvb("PMIx connect request")
    local item = tree:add(proto, tvb(), "PMIx")
    local text
    if rec.hs then
        text = "connect " .. render_connect_fields(tvb, item, rec.hs)
    else
        item:add_proto_expert_info(ef_bad, "connect request: malformed")
        text = "connect (malformed)"
    end
    item:set_text("PMIx > " .. text)
    return "> " .. text
end

function proto.dissector(tvb, pinfo, tree)
    pinfo.cols.protocol = "PMIx"
    local ifid = f_ifid()
    ifid = ifid and ifid.value or 0
    local conn = conn_get(ifid)
    local dir = pinfo.p2p_dir
    local num = pinfo.number

    if not pinfo.visited then
        if tvb:len() == 0 then
            file(num, { kind = "event" })        -- open or close
        elseif (dir == 0 or dir == 1) and not conn.broken then
            conn.buf[dir]:append(tvb():bytes())
            reassemble(conn, dir, num)
        end
    end

    local list = frames[num]
    local infos = {}
    local who = conn.rank and string.format("%s.%d", conn.ns, conn.rank)
                or ("if" .. ifid)
    if list then
        for _, rec in ipairs(list) do
            if rec.kind == "message" then
                infos[#infos + 1] = render_message(rec, tree, pinfo, conn)
            elseif rec.kind == "connect" then
                infos[#infos + 1] = render_connect(rec, tree, pinfo, conn)
            elseif rec.kind == "response" then
                local item = tree:add(proto, tvb(), "PMIx")
                local text
                if rec.status == 0 then
                    text = string.format("< connect-response status=0 "
                                         .. "pindex=%d", rec.pindex)
                else
                    text = string.format("< connect-response status=%d",
                                         rec.status)
                end
                item:add(pf.status, tvb(0, 0), rec.status):set_hidden()
                item:set_text("PMIx " .. text)
                infos[#infos + 1] = text
            elseif rec.kind == "event" then
                tree:add(proto, tvb(), "PMIx connection event")
            end
        end
    end
    if #infos > 0 then
        pinfo.cols.info = who .. ": " .. table.concat(infos, "; ")
    elseif tvb:len() > 0 then
        pinfo.cols.info = who .. ": " .. tostring(tvb:len())
            .. " bytes (continued)"
        tree:add(proto, tvb(), string.format(
            "PMIx: %d bytes, message continues", tvb:len()))
    end
end

-- The exported PDU dissector looks this up by the name in each
-- packet's dissector-name tag, which the capture writes as "pmix".
-- Registering the Proto is enough for find_dissector("pmix").
