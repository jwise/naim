if testnaim == nil then
	testnaim = {}
end

function testnaim.recvfrom(conn, name, dest, msg, flags)
	if dest == nil then
		dest = ""
	end
	conn:status_echo("[testnaim] [" .. conn.winname .. "] [" ..flags.."] <font color=\"#FFFFFF\">&lt;<font color=\"#FF0000\">"..name.."<font color=\"#FFFFFF\">@<font color=\"#00FF00\">"..dest.."<font color=\"#FFFFFF\">&gt; "..msg)
end

naim.debug("[testnaim] Functions loaded")

if testnaim.ref ~= nil then
	naim.hooks.recvfrom.del(testnaim.ref)
end

testnaim.ref = naim.hooks.recvfrom.add(testnaim.recvfrom, 99)

naim.debug("[testnaim] Hooks set")
