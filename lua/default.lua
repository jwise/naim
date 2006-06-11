naim.variables = {}
naim.commands = {}
naim.connections = {}

setmetatable(naim.connections, {
	__newindex = function (t,k,v)
		error("only naim can update the connections table",2)
	end
})

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

function naim.internal.newConn(id)
	-- Hahaha, what a hack.
	local winname = naim.prototypes.connection.get_winname(id)

	setmetatable(naim.connections, {})
	naim.connections[winname] = {
		id = id
	}
	local mt = {
		__index = function(table, key)
			if naim.prototypes.connection[key] ~= nil then
				return naim.prototypes.connection[key]
			end
			if naim.prototypes.connection["get_"..key] ~= nil then
				return naim.prototypes.connection["get_"..key](table)
			end
			return nil
		end,
		__newindex = function(table, key, value)
			if naim.prototypes.connection["set_"..key] ~= nil then
				naim.prototypes.connection["set_"..key](table, key, value)
				return
			end
			table[key] = value
		end
	}
	setmetatable(naim.connections[winname], mt);
	setmetatable(naim.connections, {
		__newindex = function (t,k,v)
			error("only naim can update the connections table",2)
		end
	})
end

function naim.internal.delConn(id)
	local winname = naim.prototypes.connection.get_winname(id)
	setmetatable(naim.connections, {})
	naim.connections[winname] = nil
	setmetatable(naim.connections, {
		__newindex = function (t,k,v)
			error("only naim can update the connections table",2)
		end
	})
end
