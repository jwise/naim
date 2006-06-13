function naim.internal.expandString(s)
	s = string.gsub(s, "$%((.*)%)",
		function (n)
			return loadstring("return "..n)()
		end)
	s = string.gsub(s, "$\{([a-zA-Z0-9:_]+)\}",
		function (n)
			return tostring(naim.variables[n])
		end)
	s = string.gsub(s, "$([a-zA-Z0-9:_]+)",
		function (n)
			return tostring(naim.variables[n])
		end)
	return s
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
	})
end

function naim.internal.rometatable(name)
	return({
		__newindex = function (t,k,v)
			error("only naim can update the " .. name .. " table", 2)
		end
	})
end



naim.variables = {}
naim.commands = {}
naim.connections = {}

setmetatable(naim.connections, naim.internal.rometatable("connections"))

function naim.internal.newConn(winname, handle)
	setmetatable(naim.connections, {})
	naim.connections[winname] = {
		handle = handle,
		windows = {},
		buddies = {},
	}
	setmetatable(naim.connections[winname], naim.internal.rwmetatable(naim.prototypes.connections))
	setmetatable(naim.connections[winname].windows, naim.internal.rometatable("windows"))
	setmetatable(naim.connections[winname].buddies, naim.internal.rometatable("buddies"))
	setmetatable(naim.connections, naim.internal.rometatable("connections"))
end

function naim.internal.delConn(winname)
	setmetatable(naim.connections, {})
	naim.connections[winname] = nil
	setmetatable(naim.connections, naim.internal.rometatable("connections"))
end

function naim.internal.newwin(conn, winname, handle)
	setmetatable(conn.windows, {})
	conn.windows[winname] = {
		handle = handle
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
		handle = handle
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
