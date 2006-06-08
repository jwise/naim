naim.variables = {}
naim.commands = {}
naim.connections = {}

setmetatable(naim.connections, { __newindex = function (t,k,v) error("only naim can update the connections table",2) end })

function naim.__expandString(s)
	s = string.gsub(s, "$\{([a-zA-Z0-9:_]+)\}", function (n) return tostring(naim.variables[n]) end)
	s = string.gsub(s, "$([a-zA-Z0-9:_]+)", function (n) return tostring(naim.variables[n]) end)
	return s
end

function naim.__newConn(id)
	-- Hahaha, what a hack.
	setmetatable(naim.connections, { })
	naim.connections[id] = { __id = id }
	local mt = {
		__index = function (table, key)
			if key == "sn" then return naim.__conn_get_sn(id) end
			if key == "password" then return naim.__conn_get_password(id) end
			if key == "winname" then return naim.__conn_get_winname(id) end
			if key == "server" then return naim.__conn_get_server(id) end
			if key == "profile" then return naim.__conn_get_profile(id) end
			error("Unknown property " .. key .. " for connection", 2);
		end,
		__newindex = function(table, key)
			error("Cannot update property " .. key .. " for connection", 2);
		end
	}
	setmetatable(naim.connections[id], mt);
	setmetatable(naim.connections, { __newindex = function (t,k,v) error("only naim can update the connections table",2) end })
end

function naim.__delConn(id)
	setmetatable(naim.connections, { })
	naim.connections[id] = nil
	setmetatable(naim.connections, { __newindex = function (t,k,v) error("only naim can update the connections table",2) end })
end

--function set_echof(s, ...)
--	naim._set_echo(string.format(s, ...));
--end
--
--function naim.commands.set(args, conn)
--	set_echof(" %-16.16s %-30s[type] Description\n", "Variable name", "Value");
--	for var,val in pairs(naim.variables) do
--		if ((val ~= "") and (val ~= 0) and (not string.find(val, ":password$"))) then
--			if (string.find(val, " ")) then
--				val = "\"" .. val .. "\""
--			end
--			desc = "[str]"
--			if string.len(val) <= 30 then
--				set_echof(" %-16.16s %-30s %s\n", var, val, desc)
--			else
--				set_echof(" %-16.16s %-30s\n", var, val)
--				set_echof(" %-16.16s %-30s %s\n", "", "", desc)
--			end
--			set_echof("\n")
--		end
--	end
--end
