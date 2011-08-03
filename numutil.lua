--  _ __   __ _ ___ __  __
-- | '_ \ / _` |_ _|  \/  | naim
-- | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>, 
-- | | | | |_| || || |  | | 2006-2007 Joshua Wise <joshua@joshuawise.com>
-- |_| |_|\__,_|___|_|  |_| ncurses-based chat client
--
-- numutil.lua
-- Various string-to-number utilities.

module("numutil", package.seeall)
function strtobe16(s, pos)
	if pos ~= nil then s = s:sub(pos) end
	return s:byte(1)*256 + s:byte(2)
end
function be16tostr(n)
	return string.char(math.floor(n/256), n%256)
end

function strtobe32(s, pos)
	if pos ~= nil then s = s:sub(pos) end
	return s:byte(1)*256*256*256 + s:byte(2)*256*256 + s:byte(3)*256 + s:byte(4)
end


inttohex = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F" }
function tohex(s)
	local outs = ""
	for i=1,s:len() do
		local c = s:byte(i)
		outs = outs .. numutil.inttohex[math.floor(c/16) + 1] .. numutil.inttohex[c%16 + 1] .. " ";
	end
	return outs
end