--  _ __   __ _ ___ __  __
-- | '_ \ / _` |_ _|  \/  | naim
-- | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
-- | | | | |_| || || |  | | Lua support Copyright 2006-2007 Joshua Wise <joshua@joshuawise.com>
-- |_| |_|\__,_|___|_|  |_| ncurses-based chat client
--
-- xkcd.lua
-- Downloads the latest xkcd RSS on request and informs of the latest xkcd.
--
-- Really badly behaved. Mostly to demonstrate sockets and their
-- functionality; they're a very poor demonstration of any other sane
-- programming techniques. In particular, if a user queues up a request
-- while another request is already running, all Hell will break loose. It
-- may be in the user's best interests to keep Hell as well confined as
-- possible.

sock = naim.socket.create()
buf = naim.buffer.create()
buf:resize(16384)

naim.hooks.add('preselect',
	function(rd, wr, ex, n)
		n = sock:preselect(rd, wr, ex, n)
		return true, n
	end, 100)
naim.hooks.add('postselect',
	function(rd, wr, ex)
		e = sock:postselect(rd, wr, ex, buf)
		if e then
			naim.echo("Postselect error: " .. e)
		end
		if buf:readdata() and buf:peek(16384):len() ~= 0 then
			naim.echo("I have " .. buf:peek(16384):len() .. " bytes of data")
			data = buf:take(16384)
			if data:match("<item>[^<]*<title>([^<]*)</title>[^<]*<link>([^<]*)") then
				title,link = data:match("<item>[^<]*<title>([^<]*)</title>[^<]*<link>([^<]*)")
				theconn:msg(thedest, thesn .. ", latest xkcd is " .. title .. " ( " .. link .. " )")
				sock:close()
			else
				naim.echo("No xkcd data!")
			end
		end
	end, 100)

naim.hooks.add('proto_recvfrom', function (conn, sn, dest, text, flags)
	theconn = conn
	thesn = sn
	thedest = dest
	if text:match("^!xkcd") then
		sock:connect("xkcd.com", 80)
		ref = naim.hooks.add('postselect',
			function(rd, wr, ex)
				if sock:connected() then
					sock:send("GET /rss.xml HTTP/1.1\nHost: xkcd.com\n\n")
					naim.hooks.del('postselect', ref)
				end
			end, 150)
	end
end, 100)
