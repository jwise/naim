naim.internal.insensitive_index = {
	__index = function(t, s)
		s = s:lower()

		for k,v in pairs(t) do
			if k:lower() == s then
				return v
			end
		end
	end,
}

setmetatable(naim.commands, naim.internal.insensitive_index)

function naim.prototypes.windows.event(window, e, s)
	if window.eventtab and window.eventtab[e] then
		window.eventtab[e].func(window, e, s)
	elseif window.eventtab and window.eventtab.default then
		window.eventtab.default.func(window, e, s)
	elseif window.conn.eventtab and window.conn.eventtab[e] then
		window.conn.eventtab[e].func(window, e, s)
	elseif window.conn.eventtab and window.conn.eventtab.default then
		window.conn.eventtab.default.func(window, e, s)
	elseif naim.eventtab[e] then
		naim.eventtab[e].func(window, e, s)
	elseif naim.eventtab.default then
		naim.eventtab.default.func(window, e, s)
	end
end

function naim.prototypes.windows.event2(window, e, who, verb, object)
	if not window.events then
		window.events = {}
	end

	if not window.events[who] then
		window.events[who] = { type = e }
	end

	local verblist

	for i,cverb in ipairs(window.events[who]) do
		if cverb.verb == verb then
			verblist = cverb
			break
		end
	end
	if not verblist then
		verblist = { verb = verb, objects = {} }
		table.insert(window.events[who], verblist)
	end

	if object and object ~= "" then
		table.insert(verblist.objects, object)
	end
end

function naim.internal.split(s, c)
	local t = {}

	for v in string.gmatch(s, '[^' .. c .. ']+') do
		table.insert(t, v)
	end

	return t
end

function naim.internal.v2k(t1)
	local t = {}

	for k,v in pairs(t1) do
		t[v] = k
	end

	return t
end

function naim.internal.dtime(_t)
	local t = math.floor(_t)

	if t < 2 then
		local mt = math.floor(100*(_t - t))

		buf = string.format("%i.%02is", _t, mt)
	elseif t < 10 then
		local mt = math.floor(10*(_t - t))

		buf = string.format("%i.%01is", _t, mt)
	elseif t < 90 then
		buf = t .. "s"
	elseif t < 60*60 then
		buf = math.floor(t/60) .. "m"
	elseif t < 24*60*60 then
		buf = math.floor(t/(60*60)) .. ":" .. string.format("%02i", math.floor(t/60%60)) .. "m"
	elseif t < 365*24*60*60 then
		buf = math.floor(t/(24*60*60)) .. "d " .. math.floor(t/(60*60)%24) .. ":" .. string.format("%02i", math.floor(t/60%60)) .. "m"
	else
		buf = math.floor(t/(365*24*60*60)) .. "y " .. math.floor(t/(24*60*60)%365) .. "d"
	end

	return buf
end

function naim.internal.eventeval(code)
	local functabfunc = assert(loadstring("return {" .. (code and code or "") .. "}"))

	setfenv(functabfunc, {
		display = function(window, e, s)
			if s then
				window:echo(s)
			end
		end,
		notify = function(window, e, s)
			window:notify()
		end,
		ignore = function(window, e, s)
		end,
		naim = naim,
		string = string,
	})

	local functab = functabfunc()

	if #functab == 0 then
		return nil
	else
		return {
			func = function(w, e, s)
				for _,f in ipairs(functab) do
					f(w, e, s)
				end
			end,
			code = code,
		}
	end
end

function naim.internal.rwmetatable(prototype)
	return({
		__index = function(t, key)
			if prototype[key] ~= nil then
				return prototype[key]
			elseif prototype["get_"..key] ~= nil then
				return prototype["get_"..key](t.handle)
			else
				return nil
			end
		end,
		__newindex = function(t, key, value)
			if prototype["set_"..key] ~= nil then
				prototype["set_"..key](t.handle, key, value)
			elseif prototype["get_"..key] ~= nil then
				error(key .. " is a read-only attribute",2)
			else
				rawset(t, key, value)
			end
		end,
		__tostring = function(t)
			return(t.name)
		end,
	})
end

function naim.internal.rometatable(name)
	return({
		__newindex = function (t,k,v)
			error("only naim can update the " .. name .. " table", 2)
		end
	})
end

naim.name = "default"

naim.variables = {}

naim.sockets = {}

naim.connections = {}
setmetatable(naim.connections, naim.internal.rometatable("connections"))

naim.timers = {}

do
	local display = naim.internal.eventeval("display")
	local notify = naim.internal.eventeval("display,notify")

	naim.eventtab = {
		message = notify,
		attacked = notify,
		attacks = notify,
		default = display,
	}
end

function naim.internal.varsub(s, t)
	s = string.gsub(s, "$%((.*)%)",
		function(n)
			return(loadstring("return " .. n)())
		end)
	s = string.gsub(s, "$\{([a-zA-Z0-9:_]+)\}",
		function(n)
			return(t[n])
		end)
	s = string.gsub(s, "$([a-zA-Z0-9:_]+)",
		function(n)
			return(t[n])
		end)
	return(s)
end

function naim.internal.expandstring(s)
	return(naim.internal.varsub(s, naim.variables))
end

function naim.internal.newconn(name, handle)
	setmetatable(naim.connections, {})
	naim.connections[name] = {
		handle = handle,
		name = name,
		windows = {},
		buddies = {},
		groups = {},
	}
	setmetatable(naim.connections[name].windows, naim.internal.insensitive_index)
	setmetatable(naim.connections[name].buddies, naim.internal.insensitive_index)
	setmetatable(naim.connections[name].groups, naim.internal.insensitive_index)
	setmetatable(naim.connections[name], naim.internal.rwmetatable(naim.prototypes.connections))
	setmetatable(naim.connections, naim.internal.rometatable("connections"))
end

function naim.internal.delconn(name)
	setmetatable(naim.connections, {})
	naim.connections[name] = nil
	setmetatable(naim.connections, naim.internal.rometatable("connections"))
end

function naim.internal.newwin(conn, winname, handle)
	conn.windows[winname] = {
		handle = handle,
		conn = conn,
		name = winname,
	}
	setmetatable(conn.windows[winname], naim.internal.rwmetatable(naim.prototypes.windows))
end

function naim.internal.delwin(conn, winname)
	conn.windows[winname] = nil
end

function naim.internal.newbuddy(conn, account, handle)
	conn.buddies[account] = {
		handle = handle,
		conn = conn,
		name = account,
	}
	setmetatable(conn.buddies[account], naim.internal.rwmetatable(naim.prototypes.buddies))
end

function naim.internal.changebuddy(conn, account, newaccount)
	conn.buddies[newaccount] = conn.buddies[account]
	conn.buddies[account] = nil
end

function naim.internal.delbuddy(conn, account)
	conn.buddies[account] = nil
end






function naim.call(tab, ...)
	if type(tab) == "string" then
		if not naim.commands[tab] then
			naim.echo("Unknown command " .. tab:upper() .. ".")
			return
		end
		tab = naim.commands[tab]
	end

	if type(tab) ~= "table" then
		error(tostring(tab) .. " is not a command table. This is a bug in a user script.")
	end

	local conn
	local min = tab.min
	local max = tab.max

	local isconn = function(conn)
		for k,v in pairs(naim.connections) do
			if v == conn then
				return(k)
			end
		end
		return(nil)
	end

	arg.n = nil

	if arg[1] == nil or type(arg[1]) == "string" then
		conn = naim.curconn()
	elseif isconn(arg[1]) then
		conn = table.remove(arg, 1)
	elseif arg[1].conn == nil then
		naim.echo("invalid first argument to " .. tostring(tab))
		return
	else
		conn = arg[1].conn
		arg[1] = tostring(arg[1])
	end

	if #arg == 1 then
		local newarg = {}
		local i = 1

		arg = arg[1]
		while i < max and arg ~= nil do
			newarg[i],arg = naim.internal.pullword(arg)
			i = i+1
		end
		assert(#newarg == i-1)
		if (arg ~= nil) then
			newarg[i] = arg
		end
		arg = newarg
	end

	if #arg > max then
		naim.echo("Command requires at most " .. max .. " arguments.")
		return
	elseif #arg < min then
		naim.echo("Command requires at least " .. min .. " arguments.")
		return
	end

	tab.func(conn, arg)
end

naim.help = {
	topics = {
		"In the top-right corner of your screen is a \"window list\" window, which will display one window for each online buddy once you have signed on. The window highlighted at the top is the current window. Use the Tab key to cycle through the listed windows, or /jump buddyname to jump directly to buddyname's window.",
		"",
		"The window list window will hide itself automatically to free up space for conversation, but it will come back if someone other than your current buddy messages you. You can press Ctrl-N to go to the next waiting (yellow) buddy when this happens.",
	},
	about = {
		"naim is a console-mode chat client.",
		"naim supports AIM, IRC, ICQ, and Lily.",
		"",
		"Copyright 1998-2006 Daniel Reed <n@ml.org>",
		"http://naim.n.ml.org/",
	},
}

naim.commands.help = {
	min = 0,
	max = 1,
	desc = "Display topical help on using naim",
	args = { "topic" },
	func = function(conn, arg)
		local found

		if #arg == 0 then
			arg[1] = "topics"
		end

		local function helpcmd(cmd, t)
			local str
			local i

			str = "<font color=\"#FF00FF\">Usage</font>: <font color=\"#00FF00\">/" .. cmd .. "</font>"
			i = 0
			while (i < t.min) do
				str = str .. " <font color=\"#00FF00\">&lt;" .. t.args[i+1] .. "&gt;</font>"
				i = i + 1
			end
			while (i < t.max) do
				str = str .. " [<font color=\"#FFFF00\">&lt;" .. t.args[i+1] .. "&gt;</font>"
				i = i + 1
			end
			i = t.min
			while (i < t.max) do
				str = str .. "]"
				i = i + 1
			end
			naim.curwin():x_hprint(str .. "<br>")
			if t.desc ~= nil then
				naim.curwin():x_hprint("<b>" .. t.desc .. "</b><br>")
			end
		end

		found = true
		if arg[1] == "keys" then
			naim.echo("Current key bindings can be viewed at any time with <font color=\"#00FF00\">/bind</font>:")
			naim.call(naim.commands.bind)
			naim.curwin():x_hprint("Key names beginning with ^ are entered by holding down the Ctrl key while pressing the listed key: ^N is Ctrl+N.<br>")
			naim.curwin():x_hprint("Key names beginning with M- are entered by holding down the Alt key while pressing the key, or by pressing Esc first, then typing the key: M-a is Alt+A.<br>")
			naim.curwin():x_hprint("IC is Ins and DC is Del on the numeric keypad. NPAGE and PPAGE are PgDn and PgUp.<br>")
			naim.curwin():x_hprint("Type <font color=\"#00FF00\">/bind &lt;keyname&gt; \"&lt;script&gt;\"</font> to change a key binding.<br>")
		elseif arg[1] == "settings" or arg[1] == "variables" then
			naim.echo("Current configuration settings can be viewed at any time with <font color=\"#00FF00\">/set</font>:")
			naim.call(naim.commands.set)
			naim.curwin():x_hprint("Type <font color=\"#00FF00\">/set &lt;varname&gt; \"&lt;new value&gt;\"</font> to change a configuration variable.<br>")
		elseif arg[1] == "commands" then
			naim.echo("Help on " .. tostring(arg[1]) .. ":")
			for cmd,t in pairs(naim.commands) do
				helpcmd(cmd, t)
				naim.curwin():x_hprint("<br>")
			end
		else
			found = false
		end

		if naim.commands[arg[1]] ~= nil then
			if not found then
				naim.echo("Help on " .. tostring(arg[1]) .. ":")
				found = true
			end
			helpcmd(arg[1], naim.commands[arg[1]])
		end

		if type(naim.help[arg[1]]) == "string" then
			if not found then
				naim.echo("Help on " .. tostring(arg[1]) .. ":")
				found = true
			end
			naim.curwin():x_hprint(naim.help[arg[1]] .. "<br>")
		elseif type(naim.help[arg[1]]) == "table" then
			if not found then
				naim.echo("Help on " .. tostring(arg[1]) .. ":")
				found = true
			end
			for i,v in ipairs(naim.help[arg[1]]) do
				naim.curwin():x_hprint(v .. "<br>")
			end
		end

		if not found then
			naim.echo("No help available for " .. arg[1] .. ".")
		--else
		--	naim.echo("Use the scroll keys (PgUp and PgDn or Ctrl-R and Ctrl-Y) to view the entire help.")
		end
	end,
}

naim.commands.listevents = {
	min = 0,
	max = 0,
	desc = "EXPERIMENTAL List active event handlers",
	args = {},
	func = function(conn, arg)
		local seen = {}

		naim.echo("Rules used in the current scope:")

		for _,scope in ipairs({ conn:curwin() ~= conn and conn:curwin() or {}, conn, naim }) do
			if scope.eventtab then
				for e,t in pairs(scope.eventtab) do
					if not seen[e] then
						seen[e] = true

						local s = "Scope " .. scope.name .. " event " .. e .. ":"

						naim.echo("&nbsp;&nbsp;" .. s .. string.rep("&nbsp;", 30-s:len()) .. "&nbsp;" .. t.code .. " (" .. tostring(t.func) .. ")")
					end
				end
			end
		end
	end,
}

naim.commands.on = {
	min = 2,
	max = 3,
	desc = "EXPERIMENTAL Event behavior control",
	args = { "target", "event", "action" },
	func = function(conn, arg)
		local target, e, code = unpack(arg)

		local function findscope()
			local dot = string.find(target, ':')

			if dot then
				local c, w = target:sub(1, dot-1), target:sub(dot+1)

				if not naim.connections[c] then
					error(c .. " is not a valid connection name")
				elseif not w or w == "" then
					return naim.connections[c]
				elseif not naim.connections[c].windows[w] then
					error(w .. " is not a valid window name in " .. c)
				else
					return naim.connections[c].windows[w]
				end
			elseif target == "default" then
				return naim
			elseif conn.windows[target] and naim.connections[target] then
				error(target .. " is both a connection name and a window name; use /on " .. tostring(conn) .. ":" .. target .. " event ... or /on " .. target .. ": event ...")
			elseif conn.windows[target] then
				return conn.windows[target]
			elseif naim.connections[target] then
				return naim.connections[target]
			else
				error("Unrecognized target " .. target)
			end
		end

		local scope = findscope()

		if e == "all" then
			scope.eventtab = {}
			e = "default"
		elseif not scope.eventtab then
			scope.eventtab = {}
		end

		local evald = naim.internal.eventeval(code)

		if scope.eventtab[e] then
			if evald then
				naim.echo("Changed rule for scope " .. scope.name .. " event " .. e .. ".")
			else
				naim.echo("Removed rule for scope " .. scope.name .. " event " .. e .. ".")
			end
		elseif evald then
			naim.echo("Added rule for scope " .. scope.name .. " event " .. e .. ".")
		else
			naim.echo("No rule for scope " .. scope.name .. " event " .. e .. ".")
		end

		scope.eventtab[e] = evald
	end,
}

naim.commands.dofile = {
	min = 1,
	max = 1,
	desc = "Load a file into the Lua interpreter",
	args = { "file" },
	func = function(conn, arg)
		dofile(arg[1])
	end
}

naim.commands.names = {
	min = 0,
	max = 1,
	desc = "List the users of a group",
	args = { "group" },
	func = function(conn, arg)
		local window

		if #arg > 0 and conn.windows[arg[1]] then
			window = conn.windows[arg[1]]
		else
			window = conn:curwin()
		end

		local group = conn.groups[tostring(window)]

		if not group then
			return naim.call(naim.commands.buddylist, conn, unpack(arg))
		end

		local maxlen = 0

		for member,info in pairs(group.members) do
			local mlen = member:len() + (info.operator and 1 or 0)

			if mlen > maxlen then
				maxlen = mlen
			end
		end

		local t = {}

		for member,info in pairs(group.members) do
			local mlen = member:len() + (info.operator and 1 or 0)

			table.insert(t, (info.operator and "@" or "") .. member .. string.rep("&nbsp;", maxlen-mlen))
		end

		table.sort(t)
		--table.sort(t, function(e1, e2) return e1:lower() < e2:lower() end)

		local p = "Users in group " .. tostring(window) .. ":"

		window:echo(p .. string.rep("&nbsp;", (maxlen+1)-math.fmod(p:len(), maxlen+1)) .. table.concat(t, " "))
	end,
}





function naim.settimeout(func, delay, ...)
	local arg = {...}

	func = type(func) == "function" and func or loadstring(func)

	local t = {
		when = os.time()+delay,
		func = function()
			func(unpack(arg))
		end,
	}

	naim.timers[t] = t

	return t
end

function naim.cleartimeout(timeoutID)
	naim.timers[timeoutID] = nil
end

function naim.setinterval(func, interval, ...)
	local arg = {...}

	func = type(func) == "function" and func or loadstring(func)

	local t = {
		interval = interval,
		when = os.time() + interval,
		func = function()
			func(unpack(arg))
		end,
	}

	naim.timers[t] = t

	return t
end

naim.clearinterval = naim.cleartimeout





naim.hooks.add('proto_connected', function(conn)
	if conn.online then
		conn:echo("WARNING: Lua has just received notice that this connection has connected, but as far as we knew, it was already connected! This is a bug, and Lua scripts may be unstable.")
	end
	conn.online = os.time()
end, 100)

naim.hooks.add('proto_disconnected', function(conn, errorcode)
	conn.online = nil

	conn.groups = {}
	setmetatable(conn.groups, naim.internal.insensitive_index)

	for k,buddy in pairs(conn.buddies) do
		buddy.session = nil
	end
end, 100)

naim.hooks.add('proto_connectfailed', function(conn, err, reason)
	if conn.online then
		conn:echo("WARNING: Lua has just received notice that a connect attempt failed, but as far as we knew, the connect attempt resulted in a successful connection! This is a bug.")
	end
	conn.online = nil

	conn.groups = {}
	setmetatable(conn.groups, naim.internal.insensitive_index)

	for k,buddy in pairs(conn.buddies) do
		buddy.session = nil
	end
end, 100)

naim.hooks.add('proto_userinfo', function(conn, who, info, warnlev, signontime, idletime, class)
	local buddy = conn.buddies[who]

	conn:echo("Information about <font color=\"#00FFFF\">" .. who .. "</font>:<br><font color=\"#808080\">"
		.. (buddy and buddy.session and buddy.session.caps and "Client capabilities: <b>" .. table.concat(buddy.session.caps, ' ') .. "</b><br>" or "")
		.. (warnlev > 0 and "&nbsp; &nbsp; &nbsp; Warning level: <b>" .. warnlev .. "</b><br>" or "")
		.. (signontime > 0 and "&nbsp; &nbsp; &nbsp; &nbsp; Online time: <b>" .. naim.internal.dtime(os.time()-signontime) .. "</b><br>" or "")
		.. (idletime > 0 and "&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; Idle time: <b>" .. naim.internal.dtime(60*idletime) .. "</b><br>" or "")
		.. (info and "<hr>" .. info .. "<br><hr></font>" or "")
	)
end, 100)

naim.hooks.add('proto_buddy_nickchanged', function(conn, who, newnick)
	naim.internal.changebuddy(conn, who, newnick)

	local window = conn.windows[who]

	if window then
		window:echo("<font color=\"#00FFFF\">" .. who .. "</font> is now known as <font color=\"#00FFFF\">" .. newnick .. "</font>.")
		window.name = newnick
		assert(not conn.windows[newnick])
		conn.windows[newnick] = conn.windows[who]
		conn.windows[who] = nil
	end
end, 100)

naim.hooks.add('proto_buddy_coming', function(conn, who)
	local buddy = conn.buddies[who]
	assert(buddy)
	assert(not buddy.session)

	buddy.session = {}
end, 100)

naim.hooks.add('proto_buddy_idle', function(conn, who, idletime)
	local window = conn.windows[who]
	local buddy = conn.buddies[who]
	assert(buddy)
	assert(buddy.session)

	local idle = idletime >= 10 and true or nil

	if window then
		if idle and not buddy.session.idle then
			window:event2("event", who, "is now", "idle")
		elseif not idle and buddy.session.idle then
			window:event2("event", who, "is no longer", "idle")
		end
	end

	buddy.session.idle = idle
end, 100)

naim.hooks.add('proto_buddy_away', function(conn, who)
	local window = conn.windows[who]
	local buddy = conn.buddies[who]
	assert(buddy)
	assert(buddy.session)

	if window then
		if not buddy.session.away then
			window:event2("event", who, "is now", "away")
		end
	end

	buddy.session.away = true
end, 100)

naim.hooks.add('proto_buddy_unaway', function(conn, who)
	local window = conn.windows[who]
	local buddy = conn.buddies[who]
	assert(buddy)
	assert(buddy.session)

	if window then
		if buddy.session.away then
			window:event2("event", who, "is no longer", "away")
		end
	end

	buddy.session.away = nil
end, 100)

naim.hooks.add('proto_buddy_capschanged', function(conn, who, caps)
	local buddy = conn.buddies[who]
	assert(buddy)
	assert(buddy.session)

	if not buddy.session.caps or table.concat(buddy.session.caps, ' ') ~= caps then
		local capst = naim.internal.split(caps, ' ')
		local window = conn.windows[who]

		if window and buddy.session.caps then
			local c1, c2 = naim.internal.v2k(buddy.session.caps), naim.internal.v2k(capst)

			for k,v in pairs(c1) do
				if not c2[k] then
					window:event2("event", who, "lost", "capability " .. k)
				end
			end

			for k,v in pairs(c2) do
				if not c1[k] then
					window:event2("event", who, "gained", "capability " .. k)
				end
			end
		end

		buddy.session.caps = capst
	end
end, 100)

naim.hooks.add('proto_chat_joined', function(conn, chat)
	assert(not conn.groups[chat])
	conn.groups[chat] = {
		members = {},
	}

	local window = conn.windows[chat]

	if window then
		window:echo("You are now participating in the " .. chat .. " group.")
	end
end, 100)

naim.hooks.add('proto_chat_synched', function(conn, chat)
	local group = conn.groups[chat]
	local window = conn.windows[chat]

	group.synched = true

	if window and naim.variables.autonames and tonumber(naim.variables.autonames) > 0 then
		naim.call(naim.commands.names, conn, chat)
	end
end, 100)

naim.hooks.add('proto_chat_left', function(conn, chat)
	assert(conn.groups[chat])
	conn.groups[chat] = nil
end, 100)

naim.hooks.add('proto_chat_kicked', function(conn, chat, by, reason)
	assert(conn.groups[chat])
	conn.groups[chat] = nil

	local window = conn.windows[chat]

	if window then
		if reason and reason ~= "" then
			window:event("attacked", "You have been kicked from group " .. chat .. " by <font color=\"#00FFFF\">" .. by .. "</font> (</b><body>" .. reason .. "</body><b>).")
		else
			window:event("attacked", "You have been kicked from group " .. chat .. " by <font color=\"#00FFFF\">" .. by .. "</font>.")
		end
	end
end, 100)

naim.hooks.add('proto_chat_oped', function(conn, chat, by)
	local group = conn.groups[chat]

	group.operator = true

	if group.synched then
		local window = conn.windows[chat]

		if window then
			window:event("attacked")
		end
	end
end, 100)

naim.hooks.add('proto_chat_deoped', function(conn, chat, by)
	local group = conn.groups[chat]

	group.operator = nil

	assert(group.synched)
	local window = conn.windows[chat]

	if window then
		window:event("attacked")
	end
end, 100)

naim.hooks.add('proto_chat_modeset', function(conn, chat, by, mode, arg)
	local group = conn.groups[chat]
	local window = conn.windows[chat]

	if window and group.synched then
		window:event2("event", by, "set", "<font color=\"#FF00FF\">" .. string.lower(mode) .. (arg and " " .. arg or "") .. "</font>")
	end
end, 100)

naim.hooks.add('proto_chat_modeunset', function(conn, chat, by, mode, arg)
	local group = conn.groups[chat]
	local window = conn.windows[chat]

	if window and group.synched then
		window:event2("event", by, "unset", "<font color=\"#FF00FF\">" .. string.lower(mode) .. (arg and " " .. arg or "") .. "</font>")
	end
end, 100)

naim.hooks.add('proto_chat_user_joined', function(conn, chat, who, extra)
	local group = conn.groups[chat]
	local window = conn.windows[chat]

	assert(not group.members[who])
	group.members[who] = {}

	if window and group.synched then
		if extra and extra ~= "" then
			window:event("users", "<font color=\"#00FFFF\">" .. who .. "</font> (" .. extra .. ") has joined group " .. chat .. ".")
		else
			window:event("users", "<font color=\"#00FFFF\">" .. who .. "</font> has joined group " .. chat .. ".")
		end
	end
end, 100)

naim.hooks.add('proto_chat_user_left', function(conn, chat, who, reason)
	local group = conn.groups[chat]
	local window = conn.windows[chat]

	assert(group.members[who])
	group.members[who] = nil

	if window then
		if reason and reason ~= "" then
			window:event("users", "<font color=\"#00FFFF\">" .. who .. "</font> has left group " .. chat .. " (</b><body>" .. reason .. "</body><b>).")
		else
			window:event("users", "<font color=\"#00FFFF\">" .. who .. "</font> has left group " .. chat .. ".")
		end
	end
end, 100)

naim.hooks.add('proto_chat_user_kicked', function(conn, chat, who, by, reason)
	local group = conn.groups[chat]
	local window = conn.windows[chat]

	assert(group.members[who])
	group.members[who] = nil

	if window then
		if reason and reason ~= "" then
			window:event("attacks", "<font color=\"#00FFFF\">" .. who .. "</font> has been kicked out of group " .. chat .. " by <font color=\"#00FFFF\">" .. by .. "</font> (</b><body>" .. reason .. "</body><b>).")
		else
			window:event("attacks", "<font color=\"#00FFFF\">" .. who .. "</font> has been kicked out of group " .. chat .. " by <font color=\"#00FFFF\">" .. by .. "</font>.")
		end
	end
end, 100)

naim.hooks.add('proto_chat_user_oped', function(conn, chat, who, by)
	local group = conn.groups[chat]

	group.members[who].operator = true

	if group.synched then
		local window = conn.windows[chat]

		if window then
			window:event2("attacks", by, "oped", "<font color=\"#00FFFF\">" .. who .. "</font>")
		end
	end
end, 100)

naim.hooks.add('proto_chat_user_deoped', function(conn, chat, who, by)
	local group = conn.groups[chat]

	group.members[who].operator = nil

	assert(group.synched)
	local window = conn.windows[chat]

	if window then
		window:event2("attacks", by, "deoped", "<font color=\"#00FFFF\">" .. who .. "</font>")
	end
end, 100)

naim.hooks.add('proto_chat_user_nickchanged', function(conn, chat, who, newnick)
	local group = conn.groups[chat]
	local window = conn.windows[chat]

	assert(not group.members[newnick])
	group.members[newnick] = group.members[who]
	group.members[who] = nil

	if window then
		window:event("misc", "<font color=\"#00FFFF\">" .. who .. "</font> is now known as <font color=\"#00FFFF\">" .. newnick .. "</font>.")
	end
end, 100)

naim.hooks.add('proto_chat_topicchanged', function(conn, chat, topic, by)
	local group = conn.groups[chat]
	local window = conn.windows[chat]

	group.topic = topic

	if window then
		if by and by ~= "" then
			window:event2("misc", by, "changed the topic to", "</B><body>" .. topic .. "</body><B>")
		else
			window:echo("Topic for group " .. chat .. ": </B><body>" .. topic .. "</body><B>.")
		end
	end
end, 100)

naim.hooks.add('periodic', function(now, nowf)
	--naim.settimeout(naim.echo, 5, "timer1 called")
	--naim.settimeout(function() naim.echo("timer2 called") end, 6)
	--naim.settimeout('naim.echo("timer3 called")', 7)
end, 100)

--naim.setinterval(naim.echo, 10, "interval1 called")
--naim.setinterval(function() naim.echo("interval2 called") end, 11)
--naim.setinterval('naim.echo("interval3 called")', 12)

naim.hooks.add('preselect', function(rfd, wfd, efd, maxfd, timeout)
	local now = os.time()
	local firsttimer = now + timeout

	for k,timer in pairs(naim.timers) do
		if timer.when > now and timer.when < firsttimer then
			firsttimer = timer.when
		end
	end

	return true,maxfd,(firsttimer - now)
end, 100)

naim.hooks.add('postselect', function(rfd, wfd, efd)
	local now = os.time()

	for k,timer in pairs(naim.timers) do
		if timer.when <= now then
			if timer.interval then
				timer.when = now + timer.interval
			else
				naim.cleartimeout(k)
			end
			timer.func()
			now = os.time()
		end
	end
end, 100)

naim.hooks.add('postselect', function(rfd, wfd, efd)
	for connname,conn in pairs(naim.connections) do
		for winname,window in pairs(conn.windows) do
			if window.events then
				for who,events in pairs(window.events) do
					local t = { "<font color=\"#00FFFF\">" .. who .. "</font> " }

					local objfunc = function(verb, objects)
						table.insert(t, verb)

						if #objects == 1 then
							table.insert(t, " ")
							table.insert(t, objects[1])
						elseif #objects == 2 then
							table.insert(t, " ")
							table.insert(t, objects[1])
							table.insert(t, " and ")
							table.insert(t, objects[2])
						elseif #objects > 2 then
							table.insert(t, " ")
							local last = table.remove(objects, #objects)

							table.insert(t, table.concat(objects, ", "))
							table.insert(t, ", and " .. last)
						end
					end

					if #events == 1 then
						objfunc(events[1].verb, events[1].objects)
					elseif #events == 2 then
						objfunc(events[1].verb, events[1].objects)
						table.insert(t, " and ")
						objfunc(events[2].verb, events[2].objects)
					else
						for i = 1,#events-1 do
							objfunc(events[i].verb, events[i].objects)
							table.insert(t, ", ")
						end
						table.insert(t, "and ")
						objfunc(events[#events].verb, events[#events].objects)
 					end

					window:event(events.type, table.concat(t) .. ".")
				end

				window.events = nil
			end
		end
	end
end, 100)
