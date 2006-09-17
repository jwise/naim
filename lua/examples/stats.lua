
if not stats then stats = {db={} } end


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

function stats.parse(conn, sn, dest, text, flags)

	if not dest then speaker = sn else speaker = dest end



	--cmd = string.match(text,"^!track (.*)")
	--if cmd then
	--	if stats.db[cmd] then conn:msg(speaker, "[stats] I'm already monitoring that word")
	--	else
	--		stats.db[cmd] = {count=0, last=0}
	--		conn:msg(speaker,"[stats] Now monitoring that word")
	--	end
	--
	--	end

	cmd = string.match(text, "^!stat (.*)")
	if cmd then
		cmd = string.lower(cmd)
		if stats.db[cmd] then
			conn:msg(speaker,"[stats] I've seen '"..cmd.."' a total of " .. stats.db[cmd].count .. " times, the most recent being ".. prettydiff(os.time() - stats.db[cmd].last).. " ago.")
			return
		else
			conn:msg(speaker,"[stats] I have never seen that word")
			return
		end
	end

	for w in string.gmatch(text, "[%w]+") do
		w = string.lower(w)
		if not stats.db[w] then stats.db[w]={count=0,last=0} end
		stats.db[w].count = stats.db[w].count + 1
		stats.db[w].last = os.time()
	end

	--for k,v in pairs(stats.db) do
	--	if string.find(text,k) then
	--		stats.db[k].count = stats.db[k].count + 1
	--		stats.db[k].last=os.time()
	--	end

	--end


end

if stats.ref then naim.hooks.del('proto_recvfrom', stats.ref) end

stats.ref = naim.hooks.add('proto_recvfrom', stats.parse,200)

naim.echo("[stats] loaded.")
