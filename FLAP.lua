--  _ __   __ _ ___ __  __
-- | '_ \ / _` |_ _|  \/  | naim
-- | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>, 
-- | | | | |_| || || |  | | 2006-2007 Joshua Wise <joshua@joshuawise.com>
-- |_| |_|\__,_|___|_|  |_| ncurses-based chat client
--
-- OSCAR/FLAP.lua
-- FLAP dispatch routines for OSCAR PD

local _tostring = tostring
module("OSCAR.FLAP", package.seeall)

local mt = getmetatable(OSCAR.FLAP) or {}
function mt:__call(obj)
	if type(obj) == "string" then
		return self:fromstring(obj)
	end
	if type(obj) == "table" then
		return self:new(obj)
	end
	error("TLV cannot convert that")
end
setmetatable(OSCAR.FLAP, mt)

function OSCAR.FLAP:new(obj)
	local o = obj or {}
	setmetatable(o, { __index = self, __concat = mt.concat, __tostring = tostring })
	if o.data then
		o.data = _tostring(o.data)		-- do any necessary coercions
	end
	return o
end

function OSCAR.FLAP:tostring()
	return string.char(0x2A, self.channel) .. numutil.be16tostr(self.seq) ..
		numutil.be16tostr(self.data:len()) .. self.data
end

function OSCAR.FLAP:fromstring(s)
	if s:byte(1) ~= 0x2A then
		OSCAR:error("[FLAP] FLAP rejected due to lack of 0x2A")
		return nil
	end
	if numutil.strtobe16(s, 5) > (s:len() + 6) then
		OSCAR:error("[FLAP] FLAP rejected due to lack of data")
		return nil
	end
	local o = self:new()
	o.channel = s:byte(2)
	o.seq = numutil.strtobe16(s, 3)
	o.data = s:sub(7, numutil.strtobe16(s, 5) + 7)
	return o
end

function OSCAR.FLAP:lengthfromstring(s)
	if s:len() < 6 then
		return nil
	end
	if s:byte(1) ~= 0x2A then
		return nil
	end
	return numutil.strtobe16(s, 5) + 6
end