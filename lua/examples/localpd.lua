--  _ __   __ _ ___ __  __
-- | '_ \ / _` |_ _|  \/  | naim
-- | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>, 
-- | | | | |_| || || |  | |           2006-2007 Joshua Wise <joshua@joshuawise.com>
-- |_| |_|\__,_|___|_|  |_| ncurses-based chat client
--
-- localpd.lua
-- Demo PD that doesn't manage sockets in the conventional fashion.
--
-- Yet another bad hack. Still, it works. Try:
-- /newconn test localpd
-- /test:connect blah
-- (in a new shell) $ nc -l -p 7777 -v -v -v
-- /test:join localhost:7777
-- /test:jump localhost:7777
-- (in the nc session that you invoked) "Hello!"

function registerpd()
	local myproto = {
		name = "localpd"
	}

	function myproto:im_add_buddy(account, group, friendly)
		-- blah
		self:buddyadded(account, group, friendly)
		self:im_buddyonline(account, 1)
		return 0
	end
	function myproto:im_send_message(target, text, isauto)
		return 0
	end
	function myproto:comparenicks(a, b)
	--	naim.echo(self.name .. ": compare " .. a .. " with " .. b)	-- if you output here, you cause hatch
		if a:lower() == b:lower() then return 0 end
		return -1
	end
	function myproto:chat_join(group)
		if self.chats[group] then
			return 0
		end
		if not group:match("([a-z0-9\.]*):([0-9]*)") then
			naim.echo("bad group")
			return 0
		end
		server,port = group:match("([a-z0-9\.]*):([0-9]*)")
		naim.echo("connecting: " .. server .. " , " .. port)
		self.chats[group] = {}
		self.chats[group].sock = naim.socket.create()
		self.chats[group].buf = naim.buffer.create()
		self.chats[group].buf:resize(4096)
		self.chats[group].sock:connect(server, port)
		self.chats[group].wasconnected = false
		return 0
	end
	function myproto:chat_part(group)
		self.chats[group].sock:close()
		self.chats[group].sock:delete()
		self.chats[group].buf:delete()
		self.chats[group] = nil
		return 0
	end
	function myproto:chat_send_message(target, text, isauto)
		text = text:gsub("<br>", "\n")
		text = text:gsub("&lt;", "<")
		text = text:gsub("&gt;", ">")
		self.chats[target].sock:send(text)
		return 0
	end
	function myproto:chat_send_action(target, text, isauto)
		return 0
	end
	function myproto:room_normalize(room)	-- cute! a failure in this function will cause naim to crash!
		return room
	end
	
	function myproto:preselect(r,w,e,n)
	end
	
	function myproto:preselect_hook(r, w, e, n)	-- needs to be a hook because preselect is otherwise broken
		for k,v in pairs(self.chats) do
			n = v.sock:preselect(r,w,e,n)
		end
		return n
	end
	function myproto:postselect(r, w, e)
		for k,v in pairs(self.chats) do
			v.sock:postselect(r,w,e,v.buf)
			if not v.wasconnected and v.sock:connected() then
				self:chat_joined(k)
				v.wasconnected = true
			end
			if v.buf:readdata() and v.buf:peek(1):len() ~= 0 then
				local text = v.buf:take(4096)
				text = text:gsub("&", "&amp;")
				text = text:gsub("<", "&lt;")
				text = text:gsub(">", "&gt;")
				text = text:gsub(" ", "&nbsp;")
				text = text:gsub("\n", "<br>")
				
				self:chat_get_message(k, "remote", 0, text)
			end
			if v.wasconnected and not v.sock:connected() then
				-- blah
			end
		end
	end
	function myproto:connect(server, port, sn)
		--if not server then server = "localhost" end
		--if port == 0 then port = 1337 end
		--naim.echo(self.name .. ": Sign on to "..server..":"..port.." with "..sn)
		self:connected()
		self.preselect_hook_ref = naim.hooks.add('preselect', function(r, w, e, n) return true,self:preselect_hook(r,w,e,n) end, 100)
		return 0
	end
	function myproto:disconnect()
		naim.hooks.del('preselect', self.preselect_hook_ref)
		return 0
	end
	
	function myproto:create(type)
		o = {}
		function lookups(table, str)
			if self[str] then
				return self[str]
			end
			if naim.pd.internal[str] then
				return naim.pd.internal[str]
			end
			return nil
		end
		setmetatable(o, { __index = lookups })
		o.chats = {}
		return o
	end

	naim.pd.create(myproto)
end

registerpd()
