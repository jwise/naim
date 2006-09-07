-- IRC "seen" modules for naim
-- Copywrite 2006 Andrew Chin (achin@eminence32.net)
-- ver 2006/8/26


if not lseen then
	lseen = {
		db = {}
	}
end



function lseen.recvfrom(conn, sn, dest, text, flags)

	--naim.echo("sn: " .. sn)
	--naim.echo("flags: " .. flags)
	--naim.echo("text: " .. text)
	--naim.echo("dest: ".. dest)

	who = string.match(text,"^!seen ([%w%p`]*).*")
		
	if who then
		who = string.lower(who)

		if who == conn.sn then
			naim.echo("who:"..conn.sn)
			msg = "I'm right here!"
		else 
			msg = lseen.seen(who)
		end

		if string.lower(sn) == who then
			msg = "I see you..."
		end
		
		if dest then 	conn:msg(dest, msg) 
		else conn:msg(sn, msg) end
	end

   lseen.saw(conn, sn, dest, text, flags)

end

function lseen.join(conn, room, who, extra)
	lsn = string.lower(who)
	lseen.db[lsn] = {}
	lseen.db[lsn].lastseen = os.time()
	lseen.db[lsn].target = room
	lseen.db[lsn].event = "joining"

end

function lseen.left(conn, room, who, reason)
	lsn = string.lower(who)
	lseen.db[lsn] = {}
	lseen.db[lsn].lastseen = os.time()
	lseen.db[lsn].target = room
	lseen.db[lsn].event = "leaving"

end
	
function lseen.kick(conn, room, who, by, reason)
	lsn = string.lower(who)
	lseen.db[lsn] = {}
	lseen.db[lsn].lastseen = os.time()
	lseen.db[lsn].target = room
	lseen.db[lsn].event = "being kicked from"
end

function lseen.nick(conn, room, oldnick, newnick) 
	lsn = string.lower(oldnick)
	lseen.db[lsn] = {}
	lseen.db[lsn].lastseen = os.time()
	lseen.db[lsn].target = newnick
	lseen.db[lsn].event = "changing their nick to"

end

function lseen.saw(conn, sn, dest, text, flags)
	if dest then
		lsn = string.lower(sn)
		lseen.db[lsn] = {}
		lseen.db[lsn].lastseen = os.time()
		lseen.db[lsn].target = dest
		lseen.db[lsn].event = "talking to"
	end
	-- conn:msg(sn,"i just got the message: '".. text .. "' from '"..sn.."' to '"..dest.."'")
end

function prettydiff(sec)

	seconds = sec
   minutes = 0
	hours = nil
	
	if seconds > 59 then
		minutes = math.floor(seconds / 60)
		seconds = math.fmod(seconds, 60)
	end		

	if minutes > 59 then
		hours = math.floor(minutes/60)
		minutes = math.fmod(minutes,60)
	end

	if hours then
		return hours .. " hours, " .. minutes .. " minutes"
	else
		return minutes .. " minutes, " .. seconds .. " seconds"
	end

end

function lseen.seen(who)

	if not lseen.db[who] then
		return "I haven't seen ".. who
	end
	return who .. " was last seen " .. lseen.db[who].event .. " " .. lseen.db[who].target .. " " .. prettydiff(os.difftime(os.time(), lseen.db[who].lastseen)) .. " ago" 
	
end 

naim.commands.unloadlseen = {
	min = 0,
	max = 0, 
	args = {},
	desc = "Unload the lseen module",
	func = function (conn)
		if lseen.ref then
			naim.hooks.del('proto_recvfrom', lseen.ref)
		end
	if lseen.leftref then
		naim.hooks.del('proto_chat_user_left', lseen.leftref)
	end
	if lseen.joinref then
		naim.hooks.del('proto_chat_user_joined', lseen.joinref)
	end
	if lseen.kickref then
		naim.hooks.del('proto_chat_user_kicked', lseen.kickref)
	end
	if lseen.nickref then
		naim.hooks.del('proto_chat_user_nickchanged', lseen.nickref)
	end
	naim.echo("[lseen] unloaded.")
	end
}

function lseen.save(filename)

	filename = naim.internal.varsub(filename, { HOME = os.getenv("HOME") })
    f = io.open(filename,"w")
      if not f then
         naim.echo("[lseen] Error:  can't open that file")
         return
      end

		f:write("if not lseen then naim.echo(\"You need to load the lseen.lua script first!\") else \n")
      for k,v in pairs(lseen.db) do
         if not v.lastseen then naim.echo(k) end
         f:write("lseen.db['"..k.. "'] = {")
			for kk,vv in pairs(v) do
				f:write(kk .. " = '" .. vv .. "';\n")
			end
			f:write("}\n")
      end
		f:write("naim.echo('[lseen] seen databases imported')")
		f:write("\nend")
      f:close()
      naim.echo("[lseen] database written to ".. filename)


end

naim.commands.savelseen = {
	min = 0,
	max = 1,
	args = {'filename'},
	desc = "Save the seen database to a file",
	func = function (conn, filename) 
		if not filename[1] then
			if not naim.variables.lseendb or naim.variables.lseendb == "" then
				lseen.save("$HOME/lseen.db")
			else
				lseen.save(naim.variables.lseendb)
			end
		else
			lseen.save(filename[1])
		end

	end
}


function lseen.load (filename)

	filename = naim.internal.varsub(filename, { HOME = os.getenv("HOME") })
	dofile(filename)
	--f = io.open(filename,"r")
   --     if not f then
   --         naim.echo("[lseen] Error: can't open that file")
   --         return
   --      end

   --   for line in f:lines() do
   --      for nick, prop, val in string.gmatch(line, "([%w_`-=]+)\.([%w]+)=([%w%p]+)") do
   --         --naim.echo(nick.."."..prop.."="..val)
   --         if not lseen.db[nick] then lseen.db[nick] = {} end
   --         lseen.db[nick][prop] = val
   --      end
   --   end

	--	f:close()
	--	naim.echo("[lseen] database imported from "..filename)

end


naim.commands.loadlseen = {
	min = 0, 
	max = 1,
	args = {'filename'},
	desc = "Load a seen database from a file",
	func = function (conn, filename)
		if not filename[1] then
			if not naim.variables.lseendb or naim.variables.lseendb=="" then
				lseen.load("$HOME/lseen.db")
			else
				lseen.load(naim.variables.lseendb)
			end
		else
			lseen.load(filename[1])
		end
	end
}


function lseen.periodic(now, nowf)
	if naim.variables.lseenautosave and tonumber(naim.variables.lseenautosave) > 0 then
		if not lseen.periodiccounter then lseen.periodiccounter = 0 end
		lseen.periodiccounter = lseen.periodiccounter + 1
		if lseen.periodiccounter  >= tonumber(naim.variables.lseenautosave) then

			if not naim.variables.lseendb then
				--naim.echo("will save to $HOME/lseen.db")
				lseen.save("$HOME/lseen.db")
			else
				--naim.echo("will save to "..naim.variables.lseendb)
				lseen.save(naim.variables.lseendb)
			end
			lseen.periodiccounter = 0
		end 
	end
end

if lseen.ref then
	naim.hooks.del('proto_recvfrom', lseen.ref)
end

if lseen.leftref then
	naim.hooks.del('proto_chat_user_left', lseen.leftref)
end

if lseen.joinref then
	naim.hooks.del('proto_chat_user_joined', lseen.joinref)
end

if lseen.kickref then
	naim.hooks.del('proto_chat_user_kicked', lseen.kickref)
end

if lseen.nickref then
	naim.hooks.del('proto_chat_user_nickchanged', lseen.nickref)
end

if lseen.periodicref then
	naim.hooks.del('periodic', lseen.periodicref)
end

lseen.ref = naim.hooks.add('proto_recvfrom', lseen.recvfrom, 200)
lseen.leftref = naim.hooks.add('proto_chat_user_left', lseen.left, 200)
lseen.joinref = naim.hooks.add('proto_chat_user_joined', lseen.join, 200)
lseen.kickref = naim.hooks.add('proto_chat_user_kicked', lseen.kick, 200)
lseen.nickref = naim.hooks.add('proto_chat_user_nickchanged', lseen.nick, 200)
lseen.periodicref = naim.hooks.add('periodic',lseen.periodic, 200)
naim.echo("[lseen] naim seen module loaded. Currently tracking JOIN PART KICK QUIT NICK and messages")
naim.echo("[lseen] commands:  /unloadlseen /savelseen /loadlseen")
naim.echo("[lseen] set $lseendb to a filename and $lseenautosave to a number to enable automatic periodic write of the lseen database")
