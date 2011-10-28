--  _ __   __ _ ___ __  __
-- | '_ \ / _` |_ _|  \/  | naim
-- | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
-- | | | | |_| || || |  | | Lua support Copyright 2006 Joshua Wise <joshua@joshuawise.com>
-- |_| |_|\__,_|___|_|  |_| ncurses-based chat client
--
-- i-r-elite-script.lua
-- Incredibly annoying scriptpack for naim
--
-- Not as "well-behaved" as Eliza; if you load it twice, you'll get two
-- responses each time one of these actions is triggered, and there is no
-- way to unload it.


naim.hooks.add("proto_chat_oped", function(conn, room, by)
	naim.call(naim.commands.msg, conn, room, "THANKZSZSZS 4 TEH MAD AWPZ "..by.." <b>[i-r-elite-script]</b>")
end, 100)

naim.hooks.add("proto_chat_deoped", function(conn, room, by)
	naim.call(naim.commands.msg, conn, room, "U SUCK "..by.." <b>[i-r-elite-script]</b>")
end, 100)

naim.hooks.add("proto_chat_kicked", function(conn, room, by, reason)
	naim.call(naim.commands.join, conn, room)
	naim.call(naim.commands.msg, conn, room, "WTF WAS THAT FOR "..by.." U SUCK <b>[i-r-elite-script]</b>")
end, 100)

naim.hooks.add("proto_chat_user_joined", function(conn, room, who, extra)
	naim.call(naim.commands.msg, conn, room, "OMGHI2U "..who.." <b>[i-r-elite-script]</b>")
end, 100)

naim.hooks.add("proto_chat_user_kicked", function(conn, room, who, by, reason)
	naim.call(naim.commands.msg, conn, room, "LOL "..who.." GOT PWNED BY "..by..", NUB <b>[i-r-elite-script]</b>")
end, 100)

naim.curconn():status_echo("[i-r-elite-script] EYE YAM LOADED ^_^")
