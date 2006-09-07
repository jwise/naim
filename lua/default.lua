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
		__index = function(table, key)
			if prototype[key] ~= nil then
				return prototype[key]
			elseif prototype["get_"..key] ~= nil then
				return prototype["get_"..key](table.handle)
			else
				return nil
			end
		end,
		__newindex = function(table, key, value)
			if prototype["set_"..key] ~= nil then
				prototype["set_"..key](table.handle, key, value)
			elseif prototype["get_"..key] ~= nil then
				error(key .. " is a read-only attribute",2)
			else
				rawset(table, key, value)
			end
		end,
		__tostring = function(table)
			return(table.name)
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

function naim.internal.newconn(winname, handle)
	setmetatable(naim.connections, {})
	naim.connections[winname] = {
		handle = handle,
		name = winname,
		windows = {},
		buddies = {},
		groups = {},
	}
	setmetatable(naim.connections[winname], naim.internal.rwmetatable(naim.prototypes.connections))
	setmetatable(naim.connections[winname].windows, naim.internal.rometatable("windows"))
	setmetatable(naim.connections[winname].buddies, naim.internal.rometatable("buddies"))
	setmetatable(naim.connections, naim.internal.rometatable("connections"))
end

function naim.internal.delconn(winname)
	setmetatable(naim.connections, {})
	naim.connections[winname] = nil
	setmetatable(naim.connections, naim.internal.rometatable("connections"))
end

function naim.internal.newwin(conn, winname, handle)
	setmetatable(conn.windows, {})
	conn.windows[winname] = {
		handle = handle,
		conn = conn,
		name = winname,
	}
	setmetatable(conn.windows[winname], naim.internal.rwmetatable(naim.prototypes.windows))
	setmetatable(conn.windows, naim.internal.rometatable("windows"))
end

function naim.internal.delwin(conn, winname)
	setmetatable(conn.windows, {})
	conn.windows[winname] = nil
	setmetatable(conn.windows, naim.internal.rometatable("windows"))
end

function naim.internal.newbuddy(conn, account, handle)
	setmetatable(conn.buddies, {})
	conn.buddies[account] = {
		handle = handle,
		conn = conn,
		name = account,
	}
	setmetatable(conn.buddies[account], naim.internal.rwmetatable(naim.prototypes.buddies))
	setmetatable(conn.buddies, naim.internal.rometatable("buddies"))
end

function naim.internal.changebuddy(conn, account, newaccount)
	setmetatable(conn.buddies, {})
	conn.buddies[newaccount] = conn.buddies[account]
	conn.buddies[account] = nil
	setmetatable(conn.buddies, naim.internal.rometatable("buddies"))
end

function naim.internal.delbuddy(conn, account)
	setmetatable(conn.buddies, {})
	conn.buddies[account] = nil
	setmetatable(conn.buddies, naim.internal.rometatable("buddies"))
end






function naim.call(tab, ...)
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

naim.commands.names = {
	min = 0,
	max = 1,
	desc = "List the users of a group",
	args = { "group" },
	func = function(conn, arg)
		local window

		if #arg > 0 and conn.windows[string.lower(arg[1])] then
			window = conn.windows[string.lower(arg[1])]
		else
			window = conn:curwin()
		end

		local group = conn.groups[string.lower(tostring(window))]

		if not group then
			return naim.call(naim.commands.buddylist, conn, unpack(arg))
		end

		local maxlen = 0

		for member,info in pairs(group.members) do
			local mlen = member:len() + (info.admin and 1 or 0)

			if mlen > maxlen then
				maxlen = mlen
			end
		end

		local t = {}

		for member,info in pairs(group.members) do
			local mlen = member:len() + (info.admin and 1 or 0)

			table.insert(t, (info.admin and "@" or "") .. member .. string.rep("&nbsp;", maxlen-mlen))
		end

		local p = "Users in group " .. tostring(window) .. ":"

		window:echo(p .. string.rep("&nbsp;", (maxlen+1)-math.fmod(p:len(), maxlen+1)) .. table.concat(t, " "))
	end,
}





naim.hooks.add('proto_buddy_coming', function(conn, who)
	
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

naim.hooks.add('proto_chat_joined', function(conn, chat)
	assert(not conn.groups[string.lower(chat)])
	conn.groups[string.lower(chat)] = {
		members = {},
	}

	local window = conn.windows[string.lower(chat)]

	if window then
		window:echo("You are now participating in the " .. chat .. " group.")
	end
end, 100)

naim.hooks.add('proto_chat_synched', function(conn, chat)
	local group = conn.groups[string.lower(chat)]
	local window = conn.windows[string.lower(chat)]

	group.synched = true

	if window and naim.variables.autonames and tonumber(naim.variables.autonames) > 0 then
		naim.call(naim.commands.names, conn, chat)
	end
end, 100)

naim.hooks.add('proto_chat_left', function(conn, chat)
	assert(conn.groups[string.lower(chat)])
	conn.groups[string.lower(chat)] = nil
end, 100)

naim.hooks.add('proto_chat_kicked', function(conn, chat, by, reason)
	assert(conn.groups[string.lower(chat)])
	conn.groups[string.lower(chat)] = nil

	local window = conn.windows[string.lower(chat)]

	if window then
		if reason and reason ~= "" then
			window:event("attacked", "You have been kicked from group " .. chat .. " by <font color=\"#00FFFF\">" .. by .. "</font> (</b><body>" .. reason .. "</body><b>).")
		else
			window:event("attacked", "You have been kicked from group " .. chat .. " by <font color=\"#00FFFF\">" .. by .. "</font>.")
		end
	end
end, 100)

naim.hooks.add('proto_chat_oped', function(conn, chat, by)
	local group = conn.groups[string.lower(chat)]

	group.admin = true

	if group.synched then
		local window = conn.windows[string.lower(chat)]

		if window then
			window:event("attacked")
		end
	end
end, 100)

naim.hooks.add('proto_chat_deoped', function(conn, chat, by)
	local group = conn.groups[string.lower(chat)]

	group.admin = nil

	assert(group.synched)
	local window = conn.windows[string.lower(chat)]

	if window then
		window:event("attacked")
	end
end, 100)

naim.hooks.add('proto_chat_modeset', function(conn, chat, by, mode, arg)
	local group = conn.groups[string.lower(chat)]
	local window = conn.windows[string.lower(chat)]

	if window and group.synched then
		window:event2("event", by, "set", "mode <font color=\"#FF00FF\">" .. string.char(mode) .. (arg and " " .. arg or "") .. "</font>")
	end
end, 100)

naim.hooks.add('proto_chat_modeunset', function(conn, chat, by, mode, arg)
	local group = conn.groups[string.lower(chat)]
	local window = conn.windows[string.lower(chat)]

	if window and group.synched then
		window:event2("event", by, "unset", "mode <font color=\"#FF00FF\">" .. string.char(mode) .. (arg and " " .. arg or "") .. "</font>")
	end
end, 100)

naim.hooks.add('proto_chat_user_joined', function(conn, chat, who, extra)
	local group = conn.groups[string.lower(chat)]
	local window = conn.windows[string.lower(chat)]

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
	local group = conn.groups[string.lower(chat)]
	local window = conn.windows[string.lower(chat)]

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
	local group = conn.groups[string.lower(chat)]
	local window = conn.windows[string.lower(chat)]

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
	local group = conn.groups[string.lower(chat)]

	group.members[who].admin = true

	if group.synched then
		local window = conn.windows[string.lower(chat)]

		if window then
			window:event2("attacks", by, "oped", "<font color=\"#00FFFF\">" .. who .. "</font>")
		end
	end
end, 100)

naim.hooks.add('proto_chat_user_deoped', function(conn, chat, who, by)
	local group = conn.groups[string.lower(chat)]

	group.members[who].admin = nil

	assert(group.synched)
	local window = conn.windows[string.lower(chat)]

	if window then
		window:event2("attacks", by, "deoped", "<font color=\"#00FFFF\">" .. who .. "</font>")
	end
end, 100)

naim.hooks.add('proto_chat_user_nickchanged', function(conn, chat, who, newnick)
	local group = conn.groups[string.lower(chat)]
	local window = conn.windows[string.lower(chat)]

	assert(not group.members[newnick])
	group.members[newnick] = group.members[who]
	group.members[who] = nil

	if window then
		window:event("misc", "<font color=\"#00FFFF\">" .. who .. "</font> is now known as <font color=\"#00FFFF\">" .. newnick .. "</font>.")
	end
end, 100)

naim.hooks.add('proto_chat_topicchanged', function(conn, chat, topic, by)
	local group = conn.groups[string.lower(chat)]
	local window = conn.windows[string.lower(chat)]

	group.topic = topic

	if window then
		if by and by ~= "" then
			window:event2("misc", by, "changed the topic to", "</B><body>" .. topic .. "</body><B>")
		else
			window:echo("Topic for group " .. chat .. ": </B><body>" .. topic .. "</body><B>.")
		end
	end
end, 100)

naim.hooks.add('preselect', function(rfd, wfd, efd, maxfd)
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
						table.insert(t, " and ")
						objfunc(events[#events].verb, events[#events].objects)
 					end

					window:event(events.type, table.concat(t) .. ".")
				end

				window.events = nil
			end
		end
	end
end, 100)

--naim.hooks.add('postselect', function(rfd, wfd, efd)
--	for k,v in pairs(naim.sockets) do
--	end
--end, 100)
