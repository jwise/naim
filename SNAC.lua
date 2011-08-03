--  _ __   __ _ ___ __  __
-- | '_ \ / _` |_ _|  \/  | naim
-- | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>, 
-- | | | | |_| || || |  | | 2006-2007 Joshua Wise <joshua@joshuawise.com>
-- |_| |_|\__,_|___|_|  |_| ncurses-based chat client
--
-- OSCAR/SNAC.lua
-- SNAC dispatch routines for OSCAR PD

local _tostring = tostring

module("OSCAR.SNAC",package.seeall)

local mt = getmetatable(OSCAR.SNAC) or {}
function mt:__call(obj)
	if type(obj) == "string" then
		return self:fromstring(obj)
	end
	if type(obj) == "table" then
		return self:new(obj)
	end
	error("SNAC cannot convert that")
end

function OSCAR.SNAC:new(obj)
	local o = obj or {}
	setmetatable(o, { __index = self, __tostring = tostring })
	if o.data then
		o.data = _tostring(o.data)		-- do any necessary coercions
	end
	return o
end

function OSCAR.SNAC:tostring()
	return numutil.be16tostr(self.family) ..
	       numutil.be16tostr(self.subtype) ..
	       string.char(self.flags0, self.flags1) ..
	       numutil.be16tostr(math.floor(self.reqid / (256 * 256))) ..
	       numutil.be16tostr(self.reqid % (256 * 256)) ..
	       self.data
end

function OSCAR.SNAC:fromstring(s)
	local o = self:new()
	o.family = numutil.strtobe16(s, 1)
	o.subtype = numutil.strtobe16(s, 3)
	o.flags0 = s:byte(5)
	o.flags1 = s:byte(6)
	o.reqid = numutil.strtobe16(s, 7)*256*256 + numutil.strtobe16(s, 9)
	o.data = s:sub(11)
	
	-- If the upper bit of flags0 is set, we have to strip a TLV off the front
	if naim.bit._and(o.flags0, 0x80) == 0x80 then
		o.frontdata = o.data:sub(1, numutil.strtobe16(o.data))
		o.data = o.data:sub(numutil.strtobe16(o.data) + 3)
	end
	
	return o
end
