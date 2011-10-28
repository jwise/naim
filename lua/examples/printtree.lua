function naim.printtree(n)
	local visited = {}

	function doprint(t, n, depth)
		function printkey(k)
			if type(k) == "string" then
				return(k)
			else
				return("[" .. tostring(k) .. "]")
			end
		end

		for k,v in pairs(t) do
			if type(v) == "table" then
				naim.echo(string.rep(" &nbsp;", depth) .. " " .. printkey(k) .. " = {")
				if visited[v] == nil then
					visited[v] = n .. "." .. k
					doprint(v, visited[v], depth+1)
				else
					naim.echo(string.rep(" &nbsp;", depth+1) .. " (already displayed as " .. visited[v] .. ")")
					visited[v] = visited[v] .. ", " .. n .. "." .. k
				end
				naim.echo(string.rep(" &nbsp;", depth) .. " },")
			elseif type(v) == "string" then
				naim.echo(string.rep(" &nbsp;", depth) .. " " .. printkey(k) .. " = \"" .. tostring(v) .. "\",")
			else
				naim.echo(string.rep(" &nbsp;", depth) .. " " .. printkey(k) .. " = " .. tostring(v) .. ",")
			end
		end
	end

	doprint(_G[n], n, 0)
end
