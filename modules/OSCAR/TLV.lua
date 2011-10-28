--  _ __   __ _ ___ __  __
-- | '_ \ / _` |_ _|  \/  | naim
-- | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>,
-- | | | | |_| || || |  | | 2006-2007 Joshua Wise <joshua@joshuawise.com>
-- |_| |_|\__,_|___|_|  |_| ncurses-based chat client
--
-- OSCAR/TLV.lua
-- TLV dispatch routines for OSCAR PD

local _tostring = tostring

module("OSCAR.TLV",package.seeall)
require"numutil"

local mt = getmetatable(OSCAR.TLV) or {}
function mt:__call(obj)
	if type(obj) == "string" then
		return self:fromstring(obj)
	end
	if type(obj) == "table" then
		return self:new(obj)
	end
	error("TLV cannot convert that")
end

function mt:__tostring()
	return self:tostring()
end

function mt.concat(a,b)
	return _tostring(a).._tostring(b)
end

function OSCAR.TLV:new(obj)
	local o = obj or {}
	setmetatable(o, { __index = self, __concat = mt.concat, __tostring = tostring })
	if o.value then
		o.value = _tostring(o.value)		-- do any necessary coercions
	end
	return o
end

function OSCAR.TLV:tostring()
	return numutil.be16tostr(self.type) ..
	       numutil.be16tostr(self.value:len()) .. self.value
end

function OSCAR.TLV:fromstring(s)
	local type = numutil.strtobe16(s)
	local length = numutil.strtobe16(s, 3)
	if (length + 4) > (s:len()) then
		return nil
	end
	local value = s:sub(5, length+4)

	local o = self:new()
	o.type = type
	o.value = value
	return o
end

function OSCAR.TLV:lengthfromstring(s)
	if s:len() < 4 then
		return nil
	end
	return numutil.strtobe16(s, 3) + 4
end

function OSCAR.TLV:len()
	return self.value:len() + 4
end
