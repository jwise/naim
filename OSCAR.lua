--  _ __   __ _ ___ __  __
-- | '_ \ / _` |_ _|  \/  | naim
-- | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>, 
-- | | | | |_| || || |  | | 2006-2011 Joshua Wise <joshua@joshuawise.com>
-- |_| |_|\__,_|___|_|  |_| ncurses-based chat client
--
-- oscar.lua
-- OSCAR protocol driver
--

module("OSCAR", package.seeall)
require"numutil"
require"OSCAR"

OSCAR.required_version = 4

OSCAR.name = "OSCAR"		-- remember that this guy gets sourced and used as the PD itself!
OSCAR.server = "login.oscar.aol.com"
OSCAR.port = 5190
OSCAR.buffersize = 4096

if not naim.version or naim.version() < OSCAR.required_version then
	_G.error("This package requires naim Lua API version "..OSCAR.required_version..", but this version of naim is too old.  Bailing out.")
end

-------------------------------------------------------------------------------
-- Packet format decoders / helper routines
-------------------------------------------------------------------------------

OSCAR.DEBUG =		0x01
OSCAR.NOTICE = 		0x02
OSCAR.WARNING =		0x04
OSCAR.ERROR =		0x08
OSCAR.FATAL = 		0x10
echodescriptions = {
	[OSCAR.DEBUG] = "[debug] ",
	[OSCAR.NOTICE] = "<font color=\"#00FFFF\"><b>[notice]</b></font> ",
	[OSCAR.WARNING] = "<body bgcolor=\"#FFFF00\"><b>[warning]</b></body> ",
	[OSCAR.ERROR] = "<body bgcolor=\"#FF0000\"><b>[error]</b></body> ",
	[OSCAR.FATAL] = "<body bgcolor=\"#FF0000\"><b>[fatal error]</b></body> ",
}

debuglevel = 	0x1F
function OSCAR:echo(class, text)
	local descr = "[unknown echo level] "
	if naim.bit._and(class, self.debuglevel) == 0 then
		return
	end
	if OSCAR.echodescriptions[class] then
		descr = OSCAR.echodescriptions[class]
	end
	self:chat_getmessage(":DEBUG", "OSCAR", 0, OSCAR.echodescriptions[class]..text)
end
function OSCAR:debug(text) self:echo(OSCAR.DEBUG,text) end
function OSCAR:notice(text) self:echo(OSCAR.NOTICE,text) end
function OSCAR:warning(text) self:echo(OSCAR.WARNING,text) end
function OSCAR:error(text) self:echo(OSCAR.ERROR,text) end
OSCAR.abortmsg = "OSCAR connection panic; aborting hook"
function OSCAR:fatal(text)
	naim.echo("<br>OSCAR connection panic!<br><hr>"..
	          "An internal assertion in the OSCAR protocol driver failed: "..
	          "<br><br>"..text.."<br><br>"..
	          "Please report a bug at <a href=\"https://github.com/jwise/naim-oscar/issues\">"..
	          "https://github.com/jwise/naim-oscar/issues</a>.<br><hr>")
	self:echo(OSCAR.FATAL,text)
	self:cleanup(naim.pd.fterrors.WEIRDPACKET, text)
	_G.error(OSCAR.abortmsg)
end

function OSCAR:wrap(f)
	local ok,e = xpcall(f, function(e)
		-- Special case: we've already aborted once.  In that case,
		-- don't come up with another backtrace.
		if e:match(OSCAR.abortmsg) then
			return nil
		end
		
		local errstr = e .. "<br>" .. _G.debug.traceback():gsub("\n","<br>"):gsub("\t","&nbsp;&nbsp;")
		return errstr
	end)
	if not ok then
		if not e then
			_G.error(OSCAR.abortmsg)
		end
		self:fatal(e)
	end
	return e
end

OSCAR.debug_commands = {}

OSCAR.debug_commands["help"] = function (self, text)
	local cmds = "Available :DEBUG commands are: "
	for k,v in pairs(OSCAR.debug_commands) do cmds = cmds .. k .. " " end
	self:debug(cmds)
end

OSCAR.debug_commands["panic"] = function (self, text)
	self:fatal("requested")
end

OSCAR.debug_commands["verbose"] = function (self, text)
	local verbosity = text:match("verbose (.*)")
	if not verbosity then
		self:error("syntax: verbose VERBOSITY<br>(verbosity is a bitmask from 0 to 0x1F)")
	end
	self.debuglevel = tonumber(verbosity) or 0x1F
	self:notice("verbosity now "..self.debuglevel)
end

-- Idea cribbed from http://lua-users.org/wiki/TimeZone ; what a hack!
function OSCAR:timezone()
	local tm = os.time()
	local utc = os.date("!*t", tm)
	local ltime = os.date("*t", tm)
	ltime.isdst = false
	return os.difftime(os.time(ltime), os.time(utc))
end

require"OSCAR.FLAP"
require"OSCAR.TLV"
require"OSCAR.SNAC"

-------------------------------------------------------------------------------
-- Authorizer / signon routines
-------------------------------------------------------------------------------

OSCAR.roastkey = { 0xF3, 0x26, 0x81, 0xC4, 0x39, 0x86, 0xDB, 0x92, 0x71, 0xA3,
		   0xB9, 0xE6, 0x53, 0x7A, 0x95, 0x7C }
function OSCAR:roast(pass)
	local newstr = ""
	for pos=1,pass:len() do
		newstr = newstr .. string.char(naim.bit.xor(pass:byte(pos), self.roastkey[pos % table.getn(self.roastkey)]))
	end
	return newstr
end

function OSCAR:nextauthseq()
	if not self.authseq then
		self.authseq = 0
	end
	self.authseq = self.authseq + 1
	return self.authseq - 1
end

function OSCAR:got_data_authorizer()
	local bufdata = self.authbuf:peek()
	self:debug("[auth] Got authorizer data")
	while bufdata and OSCAR.FLAP:lengthfromstring(bufdata) and OSCAR.FLAP:lengthfromstring(bufdata) <= bufdata:len() do
		local len = OSCAR.FLAP:lengthfromstring(bufdata)
		local flap = OSCAR.FLAP:fromstring(self.authbuf:take(len))
		bufdata = self.authbuf:peek()
		self:debug("[auth] Dispatching the FLAP")
		
		-- now we can dispatch the FLAP
		if flap.channel == 1 then
			-- hopefully it is a Connection Acknowledge
			if flap.data ~= string.char(0x00, 0x00, 0x00, 0x01) then
				return self:fatal("[auth] That wasn't a connection acknowledge!")
			end
			
			self.authseq = 0
			
			self.authsock:send(
				OSCAR.FLAP{channel = 1, seq = self:nextauthseq(), data = string.char(0x00, 0x00, 0x00, 0x01)}:tostring()
				) 
			
			-- okay, it is; let's send the auth request
			self.authsock:send(
				OSCAR.FLAP{channel = 2, seq = self:nextauthseq(), data =
					OSCAR.SNAC:new({family = 0x0017, subtype = 0x0006, flags0 = 0, flags1 = 0, reqid = 1, data =
						OSCAR.TLV{type = 1, value = self.screenname} ..
						OSCAR.TLV{type = 0x4B, value = ""} ..
						OSCAR.TLV{type = 0x5A, value = ""}
					}):tostring()
				}:tostring()
				)
			self:debug("[auth] Authorizer request sent")
		elseif flap.channel == 2 then
			local snac = OSCAR.SNAC:fromstring(flap.data)
			
			if snac.family ~= 0x17 then
				return self:fatal("[auth] wrong SNAC family from authorizer")
			end
			
			if snac.subtype == 0x07 then -- SRV_AUTH_KEY_RESPONSE
				local keylen, key = numutil.strtobe16(snac.data), snac.data:sub(3)
				
				if keylen ~= key:len() then
					self:error("[auth] SRV_AUTH_KEY_RESPONSE key length did not match SNAC length")
				end
				
				local md5 = naim.bit.md5(key .. self:needpass() .. "AOL Instant Messenger (SM)")
				
				self.authsock:send(
					OSCAR.FLAP{channel = 2, seq = self:nextauthseq(), data =
						OSCAR.SNAC:new({family = 0x0017, subtype = 0x0002, flags0 = 0, flags1 = 1, reqid = 0, data =
							OSCAR.TLV{type = 0x0001, value = self.screenname} ..
							OSCAR.TLV{type = 0x0003, value = "OSCAR for Lua on naim (http://naim.n.ml.org)"} ..
							OSCAR.TLV{type = 0x0016, value = string.char(0x01, 0x09)} ..
							OSCAR.TLV{type = 0x0017, value = string.char(0x00, 0x05)} ..
							OSCAR.TLV{type = 0x0018, value = string.char(0x00, 0x01)} ..
							OSCAR.TLV{type = 0x001A, value = string.char(0x0B, 0xDC)} ..
							OSCAR.TLV{type = 0x000F, value = "en"} ..
							OSCAR.TLV{type = 0x000E, value = "us"} ..
							OSCAR.TLV{type = 0x004A, value = string.char(0x1)} ..
							OSCAR.TLV{type = 0x0025, value = md5}
						}):tostring()
					}:tostring()
					)
			elseif snac.subtype == 0x03 then -- SRV_LOGIN_REPLY
				local screenname, bosip, authcookie, email, regstatus, error, errorurl
				while snac.data and OSCAR.TLV:lengthfromstring(snac.data) and OSCAR.TLV:lengthfromstring(snac.data) <= snac.data:len() do
					local tlv = OSCAR.TLV(snac.data)
					snac.data = snac.data:sub(tlv.value:len() + 5)
					    if tlv.type == 0x0001 then screenname = tlv.value
					elseif tlv.type == 0x0005 then bosip = tlv.value
					elseif tlv.type == 0x0006 then authcookie = tlv.value
					elseif tlv.type == 0x0011 then email = tlv.value
					elseif tlv.type == 0x0013 then regstatus = tlv.value
					elseif tlv.type == 0x0004 then errorurl = tlv.value
					elseif tlv.type == 0x0008 then error = tlv.value
					end
				end
				if snac.data:len() ~= 0 then
					return self:fatal("[auth] SNAC data left over after eating TLVs ("..snac.data:len()..")")
				end
				if error or errorurl then
					return self:cleanup(naim.pd.fterrors.UNKNOWN, "Error " .. tostring(numutil.strtobe16(error)) .. " (see "..tostring(errorurl).." for more detail)")
				elseif (not screenname) or (not bosip) or (not authcookie) or (not email) or (not regstatus) then
					return self:fatal("[auth] protocol error: I didn't get back an error, and I didn't get back a full auth response. I give up.")
				else
					local bosaddr,bosport = bosip:match("([a-z0-9\.]*):([0-9]*)")
					self.screenname = screenname
					
					self:debug("[auth] connecting to BOS as "..self.screenname)
					
					self:FLAPConnect(bosip, authcookie, true)
				end
			end
		elseif flap.channel == 4 then
			self:debug("[auth] authorizer closing the connection through normal channel 4 termination")
			
			self.authsock:close()
			self.authsock = nil
			self.authbuf = nil
		else
			return self:fatal("[auth] Unknown channel from authorizer: "..flap.channel)
		end
	end
	self:debug("[auth] postselect complete")
	return 0
end

-------------------------------------------------------------------------------
-- BOS / SNAC routines
-------------------------------------------------------------------------------

function OSCAR:nextbosseq()
	self.bosseq = self.bosseq + 1
	return self.bosseq
end

function OSCAR.dispatchsubtype(t)
	return function (self, snac)
		if t[snac.subtype] then
			t[snac.subtype](self, snac)
		else
			self:error("[BOS] Unhandled SNAC (in subtype dispatcher) " .. snac.family .. ":" ..snac.subtype)
		end
	end
end	

OSCAR.snacfamilydispatch = {}

function OSCAR:FLAPConnect(bosip, cookie, required)
	local flap = {}
	local bosaddr,bosport = bosip:match("([a-z0-9\.]*):([0-9]*)")
	
	if not bosaddr then
		bosaddr = bosip
		bosport = 5190
	end
	
	self:debug("[FLAP] connecting to BOS on "..bosaddr.." port "..bosport)
	
	table.insert(self.flapconns, flap)
	
	flap.cookie = cookie
	flap.seq = 0
	flap.sock = naim.socket.new()
	flap.buf = naim.buffer.new()
	flap.buf:resize(65550)
	flap.families = {}
	flap.required = required
	
	flap.sock:connect(bosaddr, bosport)
end

function OSCAR:FLAPNewData(conn)
	local bufdata = conn.buf:peek()
	while bufdata and OSCAR.FLAP:lengthfromstring(bufdata) and OSCAR.FLAP:lengthfromstring(bufdata) <= bufdata:len() do
		local len = OSCAR.FLAP:lengthfromstring(bufdata)
		local flap = OSCAR.FLAP:fromstring(conn.buf:take(len))
		bufdata = conn.buf:peek()
		
		-- now we can dispatch the FLAP
		if flap.channel == 1 then
			-- hopefully it is a Connection Acknowledge
			if flap.data ~= string.char(0x00, 0x00, 0x00, 0x01) then
				return self:fatal("[BOS] That wasn't a connection acknowledge!")
			end
			-- okay, it is; let's send the auth request
			conn.sock:send(
				OSCAR.FLAP:new({ channel = 1, seq = 0, data =
					string.char(0x00, 0x00, 0x00, 0x01) ..
					self.TLV{type = 0x0006, value = conn.cookie}
				}):tostring()
				)
			conn.seq = 1
		elseif flap.channel == 2 then	-- data SNACs
			local snac = OSCAR.SNAC:fromstring(flap.data)

			-- Pack the conn into the SNAC so that a service
			-- signon handler can know what connection it came
			-- from.
			snac.conn = conn
			
			if OSCAR.snacfamilydispatch[snac.family] then
				OSCAR.snacfamilydispatch[snac.family](self, snac)
			else
				self:error("[BOS] Unhandled SNAC " .. snac.family .. ":" ..snac.subtype)
			end
		elseif flap.channel == 4 then
			local error, errorurl
			while flap.data and OSCAR.TLV:lengthfromstring(flap.data) and OSCAR.TLV:lengthfromstring(flap.data) <= flap.data:len() do
				local tlv = OSCAR.TLV(flap.data)
				flap.data = flap.data:sub(tlv.value:len() + 5)
				    if tlv.type == 0x0004 then errorurl = tlv.value
				elseif tlv.type == 0x0008 then error = tlv.value
				else self:error("[BOS] [disconnect] Unknown TLV type " .. tlv.type)
				end
			end
			if flap.data:len() ~= 0 then
				self:error("[BOS] [disconnect] FLAP data left over after eating TLVs ("..OSCAR.tohex(flap.data)..")")
			end
			if error or errorurl then
				self:error("[BOS] disconnecting with error: " .. tostring(error) .. "(see "..tostring(errorurl).." for more detail)")
			end
			self:cleanup(naim.pd.fterrors.DISCONNECT, "BOS disconnected")
		else
			self:error("[BOS] Unknown channel: "..flap.channel)
		end
	end
	return 0
end

function OSCAR:FLAPSend(snac)
	local conn = nil
	for _,thisconn in pairs(self.flapconns) do
		for fam,_ in pairs(thisconn.families) do
			if fam == snac.family then
				conn = thisconn
			end
		end
		if conn then break end
	end
	if not conn then
		self:debug("[FLAP] Trying to open FLAP connection for family "..snac.family)
		
		if self.flapspending[snac.family] then
			self:notice("[FLAP] Connection already pending for SNAC family "..snac.family.."... is something wrong?")
		else
			-- Send a request to open the SNAC family connection.
			self.flapspending[snac.family] = {}
			self:FLAPSend(
				OSCAR.SNAC:new{family = 0x0001, subtype = 0x0004, flags0 = 0, flags1 = 0, reqid = 0, data = 
					numutil.be16tostr(snac.family)
				})
		end
			
		self.flapspending[snac.family][snac] = true
		
		return
	end
	
	conn.sock:send(
		OSCAR.FLAP:new({channel = 2, seq = conn.seq, data = snac:tostring()})
			:tostring()
		)
	conn.seq = conn.seq + 1
end

-------------------------------------------------------------------------------
-- BOS Service Control SNACs (family 0x0001)
-------------------------------------------------------------------------------

-- I had to take some liberties with the Firetalk translations.
OSCAR.Errors = {
	[0x01] = { text = "Invalid SNAC header", fte = naim.pd.fterrors.BADPROTO },
	[0x02] = { text = "Server rate limit exceeded", fte = naim.pd.fterrors.TOOFAST },
	[0x03] = { text = "Client rate limit exceeded", fte = naim.pd.fterrors.TOOFAST },
	[0x04] = { text = "Recipient is not logged in", fte = naim.pd.fterrors.USERUNAVAILABLE },
	[0x05] = { text = "Requested service unavailable", fte = naim.pd.fterrors.SERVER },
	[0x06] = { text = "Requested service not defined", fte = naim.pd.fterrors.VERSION },
	[0x07] = { text = "Obsolete SNAC", fte = naim.pd.fterrors.VERSION },
	[0x08] = { text = "Not supported by server", fte = naim.pd.fterrors.VERSION },
	[0x09] = { text = "Not supported by client", fte = naim.pd.fterrors.WEIRDPACKET },
	[0x0A] = { text = "Refused by client", fte = naim.pd.fterrors.INCOMINGERROR },
	[0x0B] = { text = "Reply too big", fte = naim.pd.fterrors.PACKETSIZE },
	[0x0C] = { text = "Responses lost", fte = naim.pd.fterrors.INCOMINGERROR },
	[0x0D] = { text = "Request denied", fte = naim.pd.fterrors.NOPERMS },
	[0x0E] = { text = "Incorrect SNAC format", fte = naim.pd.fterrors.VERSION },
	[0x0F] = { text = "Insufficient rights", fte = naim.pd.fterrors.NOPERMS },
	[0x10] = { text = "Recipient blocked", fte = naim.pd.fterrors.USERUNAVAILABLE },
	[0x11] = { text = "Sender too evil", fte = naim.pd.fterrors.FE_BLOCKED },
	[0x12] = { text = "Receiver too evil", fte = naim.pd.fterrors.FE_BLOCKED },
	[0x13] = { text = "User temporarily unavailable", fte = naim.pd.fterrors.USERUNAVAILABLE },
	[0x14] = { text = "No match", fte = naim.pd.fterrors.NOMATCH },
	[0x15] = { text = "List overflow", fte = naim.pd.fterrors.PACKETSIZE },
	[0x16] = { text = "Request ambiguous", fte = naim.pd.fterrors.VERSION },
	[0x17] = { text = "Server queue full", fte = naim.pd.fterrors.PACKETSIZE },
	[0x18] = { text = "Not while on AOL (?)", fte = naim.pd.fterrors.WEIRDPACKET },
}
	
function OSCAR:BOSControlError(snac)
	if OSCAR.Errors[numutil.strtobe16(snac.data)] then
		self:error("[BOS] [Error] Server reports " .. OSCAR.Errors[numutil.strtobe16(snac.data)].text .. " (".. numutil.tohex(snac.data)..")")
	else
		self:error("[BOS] [Error] Unknown server error " .. numutil.tohex(snac.data))
	end
end

function OSCAR:BOSControlHostReady(snac)
	local families = 0
	
	while snac.data:len() > 1 do
		local family = numutil.strtobe16(snac.data)
		
		snac.conn.families[family] = true
		
		-- Anyone queued?
		if self.flapspending[family] then
			for txsnac,_ in pairs(self.flapspending[family]) do
				self:FLAPSend(txsnac)
			end
			self.flapspending[family] = nil
		end
		
		families = families + 1
		self:debug("[BOS] [Host Ready] Family available: "..family)
		
		snac.data = snac.data:sub(3)
	end
	self:debug("[BOS] [Host Ready] BOS connection online with "..families.." families.")
	
	-- This isn't really the right place to do this, but sometimes you
	-- gotta do what you gotta do.
	if self.isconnecting then
		self:debug("[BOS] [Host Ready] Sending rate request")
		self:FLAPSend(
			OSCAR.SNAC:new{family = 0x0001, subtype = 0x0006, flags0 = 0, flags1 = 0, reqid = 0, data = ""}
			)
	end
end

function OSCAR:BOSControlRedirectService(snac)
	local srvid, address, cookie
	
	while snac.data and OSCAR.TLV:lengthfromstring(snac.data) and OSCAR.TLV:lengthfromstring(snac.data) <= snac.data:len() do
		local tlv = OSCAR.TLV(snac.data)
		snac.data = snac.data:sub(tlv.value:len() + 5)
		    if tlv.type == 0x0005 then address = tlv.value
		elseif tlv.type == 0x0006 then cookie = tlv.value
		elseif tlv.type == 0x000D then srvid = numutil.strtobe16(tlv.value)
		end
	end
	self:debug("[BOS] [Redirect Service] Got redirect for family "..(srvid or "???").." to host "..address)
	
	self:FLAPConnect(address, cookie)
end

function OSCAR:BOSControlRateResponse(snac)
	local ratecount = snac.data:byte(1) * 256 + snac.data:byte(2)
	self:debug("[BOS] [Rate Response] Got "..ratecount.." rates")
	--local acceptedrates = ""
	--for i=1,ratecount do
	--	acceptedrates = acceptedrates .. snac.data:sub(3+(i-1)*35, 4+(i-1)*35)
	--end
	acceptedrates = string.char(0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05)
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0001, subtype = 0x0008, flags0 = 0, flags1 = 0, reqid = 1, data = 
			acceptedrates
		})
	self:debug("[BOS] [Rate Response] Sent rate acknowledge SNAC")
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0001, subtype = 0x0014, flags0 = 0, flags1 = 0, reqid = 2, data = 
			string.char(0x00, 0x00, 0x00, 0x03)
		})
	self:debug("[BOS] [Rate Response] Sent privacy SNAC")
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0001, subtype = 0x000e, flags0 = 0, flags1 = 0, reqid = 3, data = 
			""
		})
	self:debug("[BOS] [Rate Response] Sent user info request SNAC")
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0002, subtype = 0x0002, flags0 = 0, flags1 = 0, reqid = 4, data = 
			""
		})
	self:debug("[BOS] [Rate Response] Sent location limit request SNAC")
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0003, subtype = 0x0002, flags0 = 0, flags1 = 0, reqid = 5, data = 
			""
		})
	self:debug("[BOS] [Rate Response] Sent blist limit request SNAC")
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0004, subtype = 0x0004, flags0 = 0, flags1 = 0, reqid = 6, data = 
			""
		})
	self:debug("[BOS] [Rate Response] Sent ICBM param request SNAC")
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0009, subtype = 0x0002, flags0 = 0, flags1 = 0, reqid = 7, data = 
			""
		})
	self:debug("[BOS] [Rate Response] Sent privacy param request SNAC")
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0013, subtype = 0x0002, flags0 = 0, flags1 = 0, reqid = 8, data = 
			""
		})
	self:debug("[BOS] [Rate Response] Sent SSI param request SNAC")
end

function OSCAR:BOSControlURL(snac)
	self:debug("[BOS] [URL] URL data received and ignored")
end

function OSCAR:BOSControlOnlineInfo(snac)
	self:debug("[BOS] [BOS Online Info] Online info received and ignored.")
end

function OSCAR:BOSControlExtStatus(snac)
	local type = numutil.strtobe16(snac.data)
	local data = snac.data:sub(3)
	
	if type == 0x0000 or type == 0x0001 then
		self:debug("[BOS] [Extended Status] Current SSBI info received and ignored")
	elseif type == 0x0002 then
		self:debug("[BOS] [Extended Status] Current status info received and ignored")
	end
end

function OSCAR:BOSControlRateUpdate(snac)
	self:debug("[BOS] [BOS Rate Update] Rate update received and ignored.")
end

OSCAR.snacfamilydispatch[0x0001] = OSCAR.dispatchsubtype({
	[0x0001] = OSCAR.BOSControlError,
	[0x0003] = OSCAR.BOSControlHostReady,
	[0x0005] = OSCAR.BOSControlRedirectService,
	[0x0007] = OSCAR.BOSControlRateResponse,
	[0x000A] = OSCAR.BOSControlRateUpdate,
	[0x000F] = OSCAR.BOSControlOnlineInfo,
	[0x0015] = OSCAR.BOSControlURL,
	[0x0021] = OSCAR.BOSControlExtStatus,
	})

-- Why is this part of the OSERVICE foodgroup instead of the Location
-- foodgroup?  We may never know.  Thanks, AOL.
function OSCAR:set_available(avail)
	avail = avail or ""
	if avail:len() > 252 then
		avail = avail:sub(1,252)
	end

	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0001, subtype = 0x001E, flags0 = 0, flags1 = 0, reqid = 0, data = 
			OSCAR.TLV{type = 0x001D, value =
				numutil.be16tostr(0x0002) ..
				numutil.be16tostr(0x0404 + avail:len()) ..
				numutil.be16tostr(avail:len()) ..
				avail ..
				numutil.be16tostr(0x0000)}
		})
	
	return 0
end

-------------------------------------------------------------------------------
-- Location control SNACs (family 0x0002)
-------------------------------------------------------------------------------

function OSCAR:BOSLocationError(snac)
	if OSCAR.Errors[numutil.strtobe16(snac.data)] then
		self:error("[BOS] [Location Error] Server reports " .. OSCAR.Errors[numutil.strtobe16(snac.data)].text .. " (".. numutil.tohex(snac.data)..")")
	else
		self:error("[BOS] [Location Error] Unknown server error " .. numutil.tohex(snac.data))
	end
end

function OSCAR:BOSLocationLimitations(snac)
	local maxprof, maxcaps = "n/a", "n/a"
	while snac.data and OSCAR.TLV:lengthfromstring(snac.data) and OSCAR.TLV:lengthfromstring(snac.data) <= snac.data:len() do
		local tlv = OSCAR.TLV(snac.data)
		snac.data = snac.data:sub(tlv.value:len() + 5)
		    if tlv.type == 0x0001 then maxprof = numutil.strtobe16(tlv.value)
		elseif tlv.type == 0x0002 then maxcaps = numutil.strtobe16(tlv.value)
		end
	end
	self:debug("[BOS] [Location Limitations] Max profile length: "..maxprof..", maximum capabilities: "..maxcaps)
	self:set_info("LuaOSCAR user")
	self:set_caps()
	self:debug("[BOS] [Location Limitations] Sent profile et al")
end

function OSCAR:BOSLocationGotInfo(snac)
	local screenname, wlevel, tlvcount = "", 0, 0
	local away, awaytime, info, online, idle, flags = nil, nil, "", 0, 0, 0
	screenname = snac.data:sub(2, snac.data:byte(1)+1)
	snac.data = snac.data:sub(snac.data:byte(1)+2)
	wlevel = numutil.strtobe16(snac.data)
	snac.data = snac.data:sub(3)
	tlvcount = numutil.strtobe16(snac.data)
	snac.data = snac.data:sub(3)
	-- AOL lies about the TLV count; ignore it
	while snac.data and OSCAR.TLV:lengthfromstring(snac.data) and (OSCAR.TLV:lengthfromstring(snac.data) <= snac.data:len()) do
		local tlv = OSCAR.TLV(snac.data)
		snac.data = snac.data:sub(tlv.value:len()+5)	-- weh, I don't care
		self:debug("[BOS] [GotInfo] TLV type: "..string.format("%04x, length %04x", tlv.type, tlv.value:len()))
		    if tlv.type == 0x0001 then flags = numutil.strtobe16(tlv.value)
		elseif tlv.type == 0x0004 and tlv.value:len() == 2 then idle = numutil.strtobe16(tlv.value)
		elseif tlv.type == 0x0004 and tlv.value:len() ~= 2 then away = tlv.value
		elseif tlv.type == 0x0003 and tlv.value:len() == 4 then online = numutil.strtobe16(tlv.value)*65536+numutil.strtobe16(tlv.value:sub(3))
		elseif tlv.type == 0x0002 then info = tlv.value
		elseif tlv.type == 0x0027 then awaytime = self.timezone() + 7*60*60 + numutil.strtobe32(tlv.value) -- OSCAR times are in PDT
		end
	end
	if snac.data:len() > 0 then
		self:warning("[BOS] [GotInfo] Data left over after TLVs: "..numutil.tohex(snac.data))
	end
	
	self:gotinfo(screenname, info, wlevel, online, idle, flags)
	
	if away and away:len() ~= 0 then
		if awaytime then
			away = math.floor((os.time() - awaytime) / 60).." :"..away
		end
		self:subcode_reply(screenname, "AWAY", away)
	end
end

OSCAR.snacfamilydispatch[0x0002] = OSCAR.dispatchsubtype({
	[0x0001] = OSCAR.BOSLocationError,
	[0x0003] = OSCAR.BOSLocationLimitations,
	[0x0006] = OSCAR.BOSLocationGotInfo,
	})

function OSCAR.uuid_to_bytes(uuid)
	function dehex(c)
		if c >= ("A"):byte(1) and c <= ("F"):byte(1) then
			return c-("A"):byte(1) + 10
		end
		if c >= ("a"):byte(1) and c <= ("f"):byte(1) then
			return c-("a"):byte(1) + 10
		end
		if c >= ("0"):byte(1) and c <= ("9"):byte(1) then
			return c-("0"):byte(1)
		end
		self:error("unsupported character in uuid?")
	end
	return uuid:gsub("[{%-}]", ""):gsub("(..)",
		function(a)
			return string.char(dehex(a:byte(1))*16 + dehex(a:byte(2)))
		end)
end

OSCAR.caps = OSCAR.uuid_to_bytes("{0946134D-4C7F-11D1-8222-444553540000}") ..
             OSCAR.uuid_to_bytes("{0946134E-4C7F-11D1-8222-444553540000}") ..
             OSCAR.uuid_to_bytes("{09461348-4C7F-11D1-8222-444553540000}") ..
             OSCAR.uuid_to_bytes("{748F2420-6287-11D1-8222-444553540000}") ..
             OSCAR.uuid_to_bytes("{563FC809-0B6F-41BD-9F79-422609DFA2F3}")

function OSCAR:set_caps()
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0002, subtype = 0x0004, flags0 = 0, flags1 = 0, reqid = 0, data = 
			OSCAR.TLV{type = 0x0005, value=OSCAR.caps} 
		})
end

function OSCAR:set_info(info)
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0002, subtype = 0x0004, flags0 = 0, flags1 = 0, reqid = 0, data = 
			OSCAR.TLV{type = 0x0001, value='text/x-aolrtf; charset="us-ascii"'} ..
			OSCAR.TLV{type = 0x0002, value=info}
		})
end

function OSCAR:set_away(away, isauto)
	if not away then away = "" end
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0002, subtype = 0x0004, flags0 = 0, flags1 = 0, reqid = 0, data = 
			OSCAR.TLV{type = 0x0003, value='text/x-aolrtf; charset="us-ascii"'} ..
			OSCAR.TLV{type = 0x0004, value=away}
		})
	return 0
end

function OSCAR:get_info(user)
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0002, subtype = 0x0015, flags0 = 0, flags1 = 0, reqid = 0, data = 
			string.char(0x00, 0x00, 0x00, 0x07, user:len()) .. user
		})
	return 0
end

-------------------------------------------------------------------------------
-- Buddy list SNACs (family 0x0003)
-------------------------------------------------------------------------------

function OSCAR:BOSBlistError(snac)
	if OSCAR.Errors[numutil.strtobe16(snac.data)] then
		self:error("[BOS] [Blist Error] Server reports " .. OSCAR.Errors[numutil.strtobe16(snac.data)].text .. " (".. numutil.tohex(snac.data)..")")
	else
		self:error("[BOS] [Blist Error] Unknown server error " .. numutil.tohex(snac.data))
	end
end

function OSCAR:BOSBlistParameters(snac)
	local maxblistsize, maxwatchers = "n/a", "n/a"
	while snac.data and OSCAR.TLV:lengthfromstring(snac.data) and OSCAR.TLV:lengthfromstring(snac.data) <= snac.data:len() do
		local tlv = OSCAR.TLV(snac.data)
		snac.data = snac.data:sub(tlv.value:len() + 5)
		    if tlv.type == 0x0001 then maxblistsize = numutil.strtobe16(tlv.value)
		elseif tlv.type == 0x0002 then maxwatchers = numutil.strtobe16(tlv.value)
		end
	end
	self:debug("[BOS] [Blist Parameters Reply] Maximum blist size: "..maxblistsize..", maximum watchers: "..maxwatchers)
end

function OSCAR:BOSBlistAddFailed(snac)
	local uname
	uname = snac.data:sub(2, snac.data:byte(1)+1)
	self:warning("[BOS] [Blist add failed] " .. uname)
	-- XXX pass this back up to naim somehow?
end

function OSCAR:BOSBlistOnline(snac, from_offline)
	local data = snac.data
	-- 0x000B for online, 0x000C for offline
	local online = snac.subtype == 0x000B
	
	while snac.data:len() > 0 do
		local bs, uname, wlev, ntlvs, userclass, caps, idle, status
		
		local away, mobile
		
		userclass = 0x0000
		
		bsz = snac.data:byte(1)
		uname = snac.data:sub(2, bsz + 1)
		wlev = numutil.strtobe16(snac.data, bsz + 2)
		ntlvs = numutil.strtobe16(snac.data, bsz + 4)
		snac.data = snac.data:sub(bsz + 6)
		
		for i = 1,ntlvs do
			if OSCAR.TLV:lengthfromstring(snac.data) > snac.data:len() then
				self:fatal("not enough data left for TLV "..i.." in blistonline?")
			end
			local tlv = OSCAR.TLV(snac.data)
			snac.data = snac.data:sub(tlv.value:len() + 5)
			
			    if tlv.type == 0x0001 then userclass = numutil.strtobe16(tlv.value)
			elseif tlv.type == 0x0004 then idle = numutil.strtobe16(tlv.value)
			elseif tlv.type == 0x000D then caps = tlv.value
			elseif tlv.type == 0x001D then
				local tlvd = tlv.value
				self:debug("[BOS] [Blist online] Extended BART; "..numutil.tohex(tlv.value))
				while tlvd:len() ~= 0 do
					local t2, st2, l2 = numutil.strtobe16(tlvd), tlvd:byte(3), tlvd:byte(4)
					self:debug("[BOS] [Blist online] EBART type "..t2)
					tlvd = tlvd:sub(5)
					if t2 == 0x0000 then -- "buddy icon"?
					elseif t2 == 0x0001 then -- buddy icon checksum
					elseif t2 == 0x0002 then -- avail message
						if l2 >= 4 then
							local l3 = numutil.strtobe16(tlvd)
							status = tlvd:sub(3, l3+2)
							self:debug("[BOS] [Blist online] looks like we have a status! "..status)
						else
							tlvd = tlvd:sub(l2 + 1)
						end
					elseif t2 == 0x000D then -- time when the status string was set
					end
					tlvd = tlvd:sub(l2 + 1)
				end
			end
		end
		
		away = naim.bit._and(userclass, 0x0020) == 0x0020
		mobile = naim.bit._and(userclass, 0x0080) == 0x0080
		
		self:debug("[BOS] [Blist online] " .. uname ..
		           (idle and (" [idle "..idle.."]") or "") ..
		           (string.format(" [class %04x]", userclass)) ..
		           (mobile and " [mobile]" or "") ..
		           (online and " [online]" or " [offline]"))
		
		if online then
			self:im_buddyonline(uname, 1)
			self:im_buddyaway(uname, away and 1 or 0)
			self:im_buddyflags(uname, mobile and 8 or 0) -- 8 is FF_MOBILE
			self:idleinfo(uname, idle and idle or 0)
			-- warninfo?
			-- capabilities?
			-- status dedup?
			-- away message? (CTCP AWAY needs to be replaced)
			self:statusinfo(uname, (status and not away) and status or "")
		else
			self:im_buddyonline(uname, 0)
		end
		
		-- Look to see if they changed their capitalization.
		for _,group in pairs(self.groups) do
			for _,member in pairs(group.members) do
				if self:comparenicks(member.name, uname) == 0 and member.name ~= uname then
					self:user_nickchanged(member.name, uname)
					self:debug("[BOS] [Blist online] name change from "..member.name.." to "..uname)
					member.name = uname
				end
			end
		end
	end
end

OSCAR.snacfamilydispatch[0x0003] = OSCAR.dispatchsubtype({
	[0x0001] = OSCAR.BOSBlistError,
	[0x0003] = OSCAR.BOSBlistParameters,
	[0x000A] = OSCAR.BOSBlistAddFailed,
	[0x000B] = OSCAR.BOSBlistOnline,
	[0x000C] = OSCAR.BOSBlistOnline,
	})

-------------------------------------------------------------------------------
-- ICBM SNACs (family 0x0004)
-------------------------------------------------------------------------------

function OSCAR:BOSICBMError(snac)
	local err = OSCAR.Errors[numutil.strtobe16(snac.data)] or { text = "unknown", fte = naim.pd.fterrors.UNKNOWN }
	local failing = self.icbm_recent[snac.reqid]
	
	if not failing then
		self:warning("[BOS] [ICBM Error] Reqid "..snac.reqid.." doesn't match anything recent...")
	end
	self:notice("[BOS] [ICBM Error] (reqid was ".. snac.reqid .."): ".. err.text .." ["..numutil.tohex(snac.data).."]")
	naim.pd.internal.error(self, err.fte.num, failing, "ICBM error: "..err.text)
end

function OSCAR:BOSICBMParameters(snac)
	self:debug("[BOS] [ICBM Parameters Reply] Undecoded; sending set ICBM request")
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0004, subtype = 0x0002, flags0 = 0, flags1 = 0, reqid = 0, data = 
			string.char(	0x00, 0x00, -- channel
					0x00, 0x00, 0x01, 0x1B, -- message flags
					0x1F, 0x40, -- max message snac size
					0x03, 0xE7, -- max sender warning level
					0x03, 0xE7, -- max rcvr warning level
					0x00, 0x00, 0x00, 0x00)
		})
	self:debug("[BOS] [ICBM Parameters Reply] Finalizing.")
	self:doinit(self.screenname)
	
	-- SET_NICKINFO_FIELDS
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0001, subtype = 0x001E, flags0 = 0, flags1 = 0, reqid = 0, data = 
			OSCAR.TLV{type = 0x0006, value=string.char(0x00, 0x00, 0x00, 0x00)}
		})
	
	-- What foodgroups do we support?
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0001, subtype = 0x0002, flags0 = 0, flags1 = 0, reqid = 0, data = 
			--          SRVNAME - VERSION - TOOLID -- TOOLVER -
			string.char(0x00,0x13,0x00,0x04,0x01,0x10,0x06,0x29) ..
			--string.char(0x00,0x0a,0x00,0x01,0x01,0x10,0x06,0x29) ..
			string.char(0x00,0x09,0x00,0x01,0x01,0x10,0x06,0x29) ..
			string.char(0x00,0x08,0x00,0x01,0x01,0x01,0x04,0x00) ..
			string.char(0x00,0x04,0x00,0x01,0x01,0x10,0x06,0x29) ..
			string.char(0x00,0x03,0x00,0x01,0x01,0x10,0x06,0x29) ..
			string.char(0x00,0x02,0x00,0x01,0x01,0x10,0x06,0x29) ..
			string.char(0x00,0x01,0x00,0x01,0x01,0x10,0x06,0x29) ..
			""
		})
	self:connected()
	self.isconnecting = false
end

function OSCAR:BOSICBM(snac)
	local channel, screenname, senderwarninglevel, message, realmessage, auto
	auto = 0
	self:debug("[BOS] [ICBM]")
	snac.data = snac.data:sub(9)	-- skip the cookie
	channel = numutil.strtobe16(snac.data)
	snac.data = snac.data:sub(3)
	screenname = snac.data:sub(2, snac.data:byte(1)+1)
	snac.data = snac.data:sub(snac.data:byte(1)+2)
	snac.data = snac.data:sub(5)	-- skip warning level and TLV count
	
	if channel ~= 1 then
		self:error("[BOS] [ICBM] Undecoded ICBM channel "..channel)
	end
	
	while snac.data and OSCAR.TLV:lengthfromstring(snac.data) and OSCAR.TLV:lengthfromstring(snac.data) <= snac.data:len() do
		local tlv = OSCAR.TLV(snac.data)
		snac.data = snac.data:sub(tlv.value:len() + 5)
		    if tlv.type == 0x0002 then message = tlv.value
		elseif tlv.type == 0x0004 and tlv.value:len() == 0 then auto = 1
		end
	end
	if snac.data:len() > 0 then
		self:warning("[BOS] [ICBM] Data left over! "..OSCAR.tohex(snac.data))
	end
	
	-- you can't make this fucking simple, can you, AOL? message? why
	-- use the established fucking TLVs when you can create more fucking
	-- fragments inside a message to decode? fucking fuckity fuck fuck
	-- fuck.
	while message and OSCAR.TLV:lengthfromstring(message) and OSCAR.TLV:lengthfromstring(message) <= message:len() do
		local tlv = OSCAR.TLV(message)
		message = message:sub(tlv.value:len() + 5)	-- trim the first four bytes -- short charset, short subset.
		if tlv.type == 0x0101 then -- message text, version 1
			local csno = numutil.strtobe16(tlv.value)
			local cssubset = numutil.strtobe16(tlv.value:sub(2))

			realmessage = tlv.value:sub(5)
			
			if csno == 0x0000 then -- ASCII; no mangling needed
			elseif csno == 0x0003 then -- ISO-8859-1?
				self:warning("[BOS] [ICBM] WARNING: ISO-8859-1 message from "..screenname.." may not be properly decoded.")
			elseif csno == 0x0002 then
				self:warning("[BOS] [ICBM] WARNING: UTF-16 message from "..screenname.." almost *certainly* mangled.")
				realmessage = realmessage:gsub(".(.)", function(x) return x end)
			else
				self:error("[BOS] [ICBM] Unknown character set "..csno.." from "..screenname..".")
			end
		end
	end
	if message:len() > 0 then
		self:error("[BOS] [ICBM] WARNING: Data left over in the TLV TLV! "..OSCAR.tohex(message))
		self:error("[BOS] [ICBM] remaining bytes: " .. message:len() .. " needed bytes: " .. OSCAR.TLV:lengthfromstring(message))
	end
	
	realmessage = self:handle_ect(screenname, realmessage, auto == 1)
	if realmessage:len() == 0 then
		return
	end
	
	if realmessage:sub(1,4):lower() == "/me " then
		self:im_getaction(screenname, auto, realmessage:sub(5))
	elseif realmessage:match(">/[mM][eE] ") then
		self:im_getaction(screenname, auto, realmessage:match(">/[mM][eE] (.*)"))
	else
		self:im_getmessage(screenname, auto, realmessage)
	end
	self:typing(screenname, 0)
end

function OSCAR:BOSICBMMissed(snac)
	self:debug("[BOS] [ICBM Missed]")
	while snac.data:len() ~= 0 do
		local screenname, wlevel, tlvcount, nmissed
		snac.data = snac.data:sub(3)	-- ignore the channel
		screenname = snac.data:sub(2, snac.data:byte(1)+1)
		snac.data = snac.data:sub(snac.data:byte(1)+2)
		wlevel = numutil.strtobe16(snac.data)
		snac.data = snac.data:sub(3)
		tlvcount = numutil.strtobe16(snac.data)
		snac.data = snac.data:sub(3)
		for i=1,tlvcount do
			if (not snac.data) or (not OSCAR.TLV:lengthfromstring(snac.data)) or (OSCAR.TLV:lengthfromstring(snac.data) < snac.data:len()) then
				self:error("[BOS] [ICBM Missed] Something's way broken here; I ran out of TLVs.")
			end
			local tlv = OSCAR.TLV(snac.data)
			snac.data = snac.data:sub(tlv.value:len()+5)	-- weh, I don't care
		end
		nmissed = numutil.strtobe16(snac.data)
		snac.data = snac.data:sub(3)
		reason = numutil.strtobe16(snac.data)
		snac.data = snac.data:sub(3)
		self:warning("[BOS] [ICBM Missed] Missed " .. nmissed .. " ICBMs from "..screenname.." ("..wlevel.."%) due to "..reason..".")
	end
end

function OSCAR:BOSICBMTypingNotification(snac)
	self:debug("[BOS] [ICBM Typing Notification]")
	local screenname, notiftype
	snac.data = snac.data:sub(9)	-- ignore notification cookie
	snac.data = snac.data:sub(3)	-- ignore the channel
	screenname = snac.data:sub(2, snac.data:byte(1)+1)
	snac.data = snac.data:sub(snac.data:byte(1)+2)
	notiftype = numutil.strtobe16(snac.data)
	self:typing(screenname, notiftype)
end

OSCAR.snacfamilydispatch[0x0004] = OSCAR.dispatchsubtype({
	[0x0001] = OSCAR.BOSICBMError,
	[0x0005] = OSCAR.BOSICBMParameters,
	[0x0007] = OSCAR.BOSICBM,
	[0x000A] = OSCAR.BOSICBMMissed,
	[0x0014] = OSCAR.BOSICBMTypingNotification,
	})

function OSCAR:chat_send_message(target, text, isauto)
	if target == ":DEBUG" then
		text = text:gsub("<.->",""):lower()
		local kw = text:match("^([^ ]*)")
		if self.debug_commands[kw] then
			self.debug_commands[kw](self, text)
		else
			self:error("Unknown :DEBUG command "..text)
		end
		return 0
	end
	
	self:error("other chat targets ("..target..") unsupported")
end

function OSCAR:im_send_message(target, text, isauto)
	local isautotlv = ""
	if isauto == 1 then
		isautotlv = OSCAR.TLV{type = 0x0004, value = ""}
	end
	
	while true do
		local rq
		if isauto == 1 then
			rq = self:dequeue_subcode_replies(target)
		else
			rq = self:dequeue_subcode_requests(target)
		end
		if not rq then break end
		text = text .. rq
	end
	
	self.icbm_req = (self.icbm_req + 1) % 65536
	self.icbm_recent[self.icbm_req] = target
	
	-- Also clear out old ones.
	self.icbm_recent[(self.icbm_req - 16 + 65536) % 65536] = nil
	
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0004, subtype = 0x0006, flags0 = 0, flags1 = 0, reqid = self.icbm_req, data = 
			string.char(	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07, -- cookie
					0x00,0x01, -- channel
					target:len()) .. target ..
			OSCAR.TLV{type = 0x0002, value = 
				string.char(0x05,0x01,0x00,0x01,0x01,0x01,0x01)..
				numutil.be16tostr(text:len()+4)..
				string.char(0x00,0x00,0x00,0x00)..
				text} ..
			isautotlv
		})
	return 0
end

function OSCAR:im_send_action(target, text, isauto)
	return self:im_send_message(target, "/me "..text, isauto)
end

function OSCAR:im_send_reply(target, text)
	-- XXX interpolate variables
	return self:im_send_message(target, text, 1)
end

function OSCAR:htmlclean(text)
	return text:gsub("&gt;", ">")
	           :gsub("&lt;", "<")
	           :gsub("&quot;", "\"")
	           :gsub("&nbsp;", " ")
	           :gsub("&amp;", "&")
end

function OSCAR:handle_ect(from, text, isreply)
	local dosubcode = isreply and self.subcode_reply or self.subcode_request
	return text:gsub("<font ECT=\"(.-)\"></font>",
		function (str)
			local cmd,args = str:match("(.-) (.*)")
			
			if args then
				args = self:htmlclean(args)
			else
				cmd = str
				args = ""
			end

			dosubcode(self, from, cmd, args)
			return ""
		end)
end

function OSCAR:subcode_encode(cmd, msg)
	return "<font ECT=\""..cmd..(msg and (" "..msg) or "").."\"></font>"
end

-------------------------------------------------------------------------------
-- Privacy management SNACs (family 0x0009)
-------------------------------------------------------------------------------

function OSCAR:BOSPrivacyReply(snac)
	self:debug("[BOS] [Privacy reply]")
end

OSCAR.snacfamilydispatch[0x0009] = OSCAR.dispatchsubtype({
	[0x0003] = OSCAR.BOSPrivacyReply,
	})

-------------------------------------------------------------------------------
-- SSI SNACs (family 0x0013)
-------------------------------------------------------------------------------

function OSCAR:BOSSSILimitReply(snac)
	self:debug("[BOS] [SSI limit reply]")
	-- I don't really care about the answer. I do, however, want the buddy list.
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0013, subtype = 0x0004, flags0 = 0, flags1 = 0, reqid = 0, data = 
			""
		})
	self.groups = {}
	self.buddies = {}
end

OSCAR.debug_commands["request-ssi"] = function(self, text)
	self:debug("Rerequesting SSIs")
	self:BOSSSILimitReply(nil)
end

OSCAR.SSITypes = {
	BUDDY = 0x0,
	GROUP = 0x1,
	PERMIT = 0x2,
	DENY = 0x3,
	PERMITDENY = 0x4,
	PRESENCE = 0x5,
	IGNORELIST = 0xE,
	LASTUPDATE = 0xF,
	RECENTBUDDY = 0x19
}

function OSCAR:_snactorosterentry(indata)
	local ent = {}
	
	ent.itemname = indata:sub(3, numutil.strtobe16(indata) + 2)
	indata = indata:sub(numutil.strtobe16(indata) + 3)
	ent.groupid = numutil.strtobe16(indata)
	indata = indata:sub(3)
	ent.itemid = numutil.strtobe16(indata)
	indata = indata:sub(3)
	ent.type = numutil.strtobe16(indata)
	indata = indata:sub(3)
	ent.data = indata:sub(3, numutil.strtobe16(indata) + 2)
	indata = indata:sub(numutil.strtobe16(indata) + 3)
	
	local nickname, buddycomment, contents
	while ent.data and OSCAR.TLV:lengthfromstring(ent.data) and OSCAR.TLV:lengthfromstring(ent.data) <= ent.data:len() do
		local tlv = OSCAR.TLV(ent.data)
		ent.data = ent.data:sub(tlv.value:len() + 5)
		    if tlv.type == 0x0131 then ent.nickname = tlv.value
		elseif tlv.type == 0x013C then ent.buddycomment = tlv.value
		elseif tlv.type == 0x00C8 then ent.contents = tlv.value
		end
	end
	
	local types = { [0x0000] = "Buddy record", [0x0001] = "Group record", [0x0002] = "Permit record",
			[0x0003] = "Deny record", [0x0004] = "Permit/deny settings", [0x0005] = "Presence info",
			[0x000E] = "Ignore list record", [0x000F] = "Last update date", [0x0019] = "Recent buddy"}
	local thestring = "[BOS] [SSI] Item: " .. ent.itemname .. ", group " .. ent.groupid .. ", item " ..ent.itemid .. ", "
	if types[ent.type] then thestring = thestring .. types[ent.type]
	else thestring = thestring .. "Type " .. ent.type
	end
	thestring = thestring .. ", "
	if ent.data:len() > 0 then thestring = thestring .. "extra data!, " end
	if ent.nickname then thestring = thestring .. "nickname " .. ent.nickname .. ", " end
	if ent.buddycomment then thestring = thestring .. "buddycomment " .. ent.buddycomment .. ", " end
	if ent.contents then thestring = thestring .. "contents, " end
	self:debug(thestring .. "end.")
	
	return indata, ent
end

function OSCAR:_AddItem(roster, differential)
	if roster.itemid == 0 then
		if roster.type ~= OSCAR.SSITypes.GROUP then
			self:fatal("[SSI] Roster item with itemid 0, but wasn't a group?")
		end
		if not self.groups[roster.groupid] then
			self.groups[roster.groupid] = {}
		end
		self.groups[roster.groupid].members = {}
		if roster.groupid ~= 0 then
			self.groups[roster.groupid].name = roster.itemname
			if not roster.contents then roster.contents = "" end
			while roster.contents:len() > 1 do
				local buddy
				buddy = numutil.strtobe16(roster.contents)
				roster.contents = roster.contents:sub(3)
				self.groups[roster.groupid].members[buddy] = {}
			end
		else
			self.groups[roster.groupid].name = "Unassigned"
		end
	else
		-- roster.type OSCAR.SSITypes.Buddy
		if not self.groups[roster.groupid] then
			self:error("[BOS] [SSI] Item without group created first?")
			self.groups[roster.groupid] = {}
			self.groups[roster.groupid].members = {}
		end
		if not self.groups[roster.groupid].members[roster.itemid] then
			if not differential and roster.groupid ~= 0 then
				self:warning("[BOS] [SSI] Item did not exist in group, but we're not doing a differential update...")
			end
			
			-- Look for the item in other groups.
			for k,v in pairs(self.groups) do
				if v.members[roster.itemid] then
					v.members[roster.itemid] = nil
				end
			end
			
			-- Only after it's in no other groups can we add it.
			self.groups[roster.groupid].members[roster.itemid] = {}
		end
		self.groups[roster.groupid].members[roster.itemid].name = roster.itemname
		self.groups[roster.groupid].members[roster.itemid].friendly = roster.nickname
		self.groups[roster.groupid].members[roster.itemid].comment = roster.comment
		self.groups[roster.groupid].members[roster.itemid].type = roster.type
		self.groups[roster.groupid].members[roster.itemid].dirty = true
	end
end

function OSCAR:_CommitDirty()
	for k,group in pairs(self.groups) do
		if not group.name then
			group.name = "Unknown group"
		end
		self:debug("[BOS] [SSI] ["..group.name.."]")
		for k2,v in pairs(group.members) do
			if v.type ~= OSCAR.SSITypes.BUDDY then
			elseif v.friendly then
				self:debug("[BOS] [SSI] * ".. v.name .. " ("..v.friendly..")" .. (v.dirty and " [dirty]" or ""))
			elseif v.name then
				self:debug("[BOS] [SSI] * ".. v.name .. (v.dirty and " [dirty]" or ""))
			else
				self:debug("[BOS] [SSI] *** UNKNOWN ***" .. (v.dirty and " [dirty]" or ""))
			end
			if v.name and v.dirty and v.type == OSCAR.SSITypes.BUDDY then
				self:buddyadded(v.name, group.name, v.friendly)
				v.dirty = false
			end
		end
	end
end

function OSCAR:BOSSSIRosterReply(snac)
	local items, lastchange, i

	if snac.data:byte(1) ~= 0 then
		return self:fatal("[BOS] [SSI] Wrong SSI protocol version!")
	end
	snac.data = snac.data:sub(2)
	items = numutil.strtobe16(snac.data)
	snac.data = snac.data:sub(3)
	self:debug("[BOS] [SSI roster reply] flags " .. snac.flags0 .. " " .. snac.flags1 .. " items " ..items)
	
	local roster
	for i = 1,items do
		snac.data, roster = self:_snactorosterentry(snac.data)
		self:_AddItem(roster)
	end
	if naim.bit._and(snac.flags1, 1) == 0 then
		-- all done
		self:_CommitDirty()
		
		-- make it so! send the activate SSI stuff down the wire
		self:FLAPSend(
			OSCAR.SNAC:new{family = 0x0013, subtype = 0x0007, flags0 = 0, flags1 = 0, reqid = 0, data = 
				""
			})
	end
end

function OSCAR:BOSSSIRemoveItem(snac)
	if not self.ssibusy then
		self:fatal("[BOS] [SSI Remove Item] Item removed outside of transaction ... ")
	end
	self:debug("[BOS] [SSI Remove Item]")
	
	local roster
		
	while snac.data ~= "" do
		snac.data, roster = self:_snactorosterentry(snac.data)
		
		if roster.type == 0x0000 then
			if not self.groups[roster.groupid] then
				self:fatal("[BOS] [SSI Remove Item] That group ID doesn't exist ...")
			end
			self:buddyremoved(self.groups[roster.groupid].members[roster.itemid].name, self.groups[roster.groupid].name)
			self.groups[roster.groupid].members[roster.itemid] = nil
		elseif roster.type == 0x0001 then
			self.groups[roster.groupid] = nil
		end
	end
	
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0013, subtype = 0x0007, flags0 = 0, flags1 = 0, reqid = 0, data = 
			""
		})
end

function OSCAR:BOSSSIModAck(snac)
	local SSIErrors = { [0x0000] = "No errors", [0x0002] = "Item to modify not found", [0x0003] = "Item already exists",
		      [0x000A] = "Error adding item", [0x000C] = "Can't add item", [0x000D] = "Adding ICQ to AIM",
		      [0x000E] = "Requires authorization" }
	if SSIErrors[numutil.strtobe16(snac.data)] then
		local func = self.error
		if numutil.strtobe16(snac.data) == 0x000 then
			func = self.debug
		end
		-- xxx: should feed up to UA if error
		func(self, "[BOS] [SSI Mod Ack] Server reports " .. SSIErrors[numutil.strtobe16(snac.data)] .. " (".. numutil.tohex(snac.data)..")")
	else
		self:error("[BOS] [SSI Mod Ack] Unknown server error " .. numutil.tohex(snac.data))
	end
end

function OSCAR:BOSSSITransactionStart(snac)
	-- xxx: detect import mode? (4 bytes: 00 01 00 00 in SNAC)
	self:debug("[BOS] [SSI Transaction Start]")
	self.ssibusy = true
end

function OSCAR:BOSSSITransactionEnd(snac)
	self:debug("[BOS] [SSI Transaction End]")
	if not self.ssibusy then
		self:fatal("[BOS] [SSI Transaction End] Transaction ended without already being in transaction?")
	end
	self.ssibusy = false
	-- Activate new configuration
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0013, subtype = 0x0007, flags0 = 0, flags1 = 0, reqid = 0, data = 
			""
		})
end

function OSCAR:BOSSSIAdd(snac)
	self:debug("[BOS] [SSI Add]")
	if not self.ssibusy then
		self:fatal("[BOS] [SSI Add] Not within a transaction?")
	end
	
	local roster
	while snac.data ~= "" do
		snac.data, roster = self:_snactorosterentry(snac.data)
		self:_AddItem(roster, true)
	end
	self:_CommitDirty()
end

function OSCAR:BOSSSIMod(snac)
	if not self.ssibusy then
		self:fatal("[BOS] [SSI Mod] Not within a transaction?")
	end
	
	local roster
	while snac.data ~= "" do
		snac.data, roster = self:_snactorosterentry(snac.data)
		
		if roster.type == 0x0000 then
			-- Look for the old item and look up missing data from it.
			local olditem = nil
			for gid,group in pairs(self.groups) do
				for mid,member in pairs(group.members) do
					if mid == roster.itemid then
						olditem = member
					end
				end
			end
			if not olditem then
				self:fatal("[BOS] [SSI Mod] Mod, but old item ID doesn't exist")
			end
			roster.itemname = roster.itemname or olditem.name
			roster.nickname = roster.nickname or olditem.friendly
			roster.comment = roster.comment or olditem.comment
			self:_AddItem(roster, true)
			self:_CommitDirty()
		elseif roster.type == 0x0001 then
			self:debug("[BOS] [SSI Mod] Mod on groups is a no-op right now.")
		end
	end
end

OSCAR.snacfamilydispatch[0x0013] = OSCAR.dispatchsubtype({
	[0x0003] = OSCAR.BOSSSILimitReply,
	[0x0006] = OSCAR.BOSSSIRosterReply,
	[0x0008] = OSCAR.BOSSSIAdd,
	[0x0009] = OSCAR.BOSSSIMod,
	[0x000A] = OSCAR.BOSSSIRemoveItem,
	[0x000E] = OSCAR.BOSSSIModAck,
	[0x0011] = OSCAR.BOSSSITransactionStart,
	[0x0012] = OSCAR.BOSSSITransactionEnd,
	})

function OSCAR:_updategroup(id)
	local tlvdata = ""
	local tlvs = ""
	
	self:debug("[BOS] [SSI] Group update " .. self.groups[id].name)
	for k,member in pairs(self.groups[id].members) do
		tlvdata = tlvdata .. numutil.be16tostr(k)
	end
	tlvs = OSCAR.TLV{type = 0x00c8, value = tlvdata}
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0013, subtype = 0x0009, flags0 = 0, flags1 = 0, reqid = 0, data = 
			numutil.be16tostr(self.groups[id].name:len()) .. self.groups[id].name ..
			numutil.be16tostr(id) ..
			numutil.be16tostr(0x0000) ..
			numutil.be16tostr(0x0001) ..
			numutil.be16tostr(tlvs:len()) ..
			tlvs
		})
end

function OSCAR:_updatemaster()
	local tlvdata = ""
	local tlvs = ""
	
	self:debug("[BOS] [SSI] Master update")
	for k,member in pairs(self.groups) do
		tlvdata = tlvdata .. numutil.be16tostr(k)
	end
	tlvs = OSCAR.TLV{type = 0x00c8, value = tlvdata}
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0013, subtype = 0x0009, flags0 = 0, flags1 = 0, reqid = 0, data = 
			numutil.be16tostr(0x0000) .. 
			numutil.be16tostr(0x0000) ..
			numutil.be16tostr(0x0000) ..
			numutil.be16tostr(0x0001) ..
			numutil.be16tostr(tlvs:len())  ..
			tlvs
		})
end

function OSCAR:im_add_buddy(account, agroup, afriendly)
	if self:comparenicks(account, ":RAW") == 0 then
		return 0
	end
	if self.ssibusy then
		self:error("[BOS] [SSI] Attempt to add buddy in transaction")
		return 1
	end
	self:debug("[BOS] [SSI] Add buddy")
	-- Start a transaction first.
	self:debug("[BOS] [SSI] Transaction start")
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0013, subtype = 0x0011, flags0 = 0, flags1 = 0, reqid = 0, data = 
			""
		})
	local gotgroup, newbuddyid, largestgroup
	largestgroup = 0
	-- Make sure they're only in one group, and at the same time, look up the new group.
	for k,group in pairs(self.groups) do
		local maxbuddyid = 0
		
		if k > largestgroup then
			largestgroup = k
		end
		
		for k2,member in pairs(group.members) do
			if self:comparenicks(member.name, account) == 0 then
				self:debug("[BOS] [SSI] Removing "..member.name.." from old group "..group.name)
				self:FLAPSend(
					OSCAR.SNAC:new{family = 0x0013, subtype = 0x000A, flags0 = 0, flags1 = 0, reqid = 0, data = 
						numutil.be16tostr(member.name:len()) .. member.name ..
						numutil.be16tostr(k) ..
						numutil.be16tostr(k2) ..
						numutil.be16tostr(member.type) ..
						numutil.be16tostr(0x0000) -- no TLVs
					})
				group.members[k2] = nil
				self:_updategroup(k)
				if k2 > maxbuddyid then
					maxbuddyid = k2
				end
			end
		end

		if agroup:lower() == group.name:lower() then
			gotgroup = k
			newbuddyid = maxbuddyid + 1
		elseif next(group.members) == nil and k ~= 0x0000 then	-- is the group empty? if so, delete that, too
			self:debug("[BOS] [SSI] Deleting empty group " .. group.name)
			self:FLAPSend(
				OSCAR.SNAC:new{family = 0x0013, subtype = 0x000A, flags0 = 0, flags1 = 0, reqid = 0, data = 
					numutil.be16tostr(group.name:len()) .. group.name ..
					numutil.be16tostr(k) ..
					numutil.be16tostr(0x0000) ..
					numutil.be16tostr(0x0001) ..	-- group, not user
					numutil.be16tostr(0x0000) -- no TLVs
				})
			self.groups[k] = nil
			self:_updatemaster()
		end
	end
	if not gotgroup then
		gotgroup = largestgroup + 1
		self.groups[gotgroup] = {}
		self.groups[gotgroup].members = {}
		self.groups[gotgroup].name = agroup
		-- Add the group
		self:debug("[BOS] [SSI] Creating new group " .. agroup)
		self:FLAPSend(
			OSCAR.SNAC:new{family = 0x0013, subtype = 0x0008, flags0 = 0, flags1 = 0, reqid = 0, data = 
				numutil.be16tostr(agroup:len()) .. agroup ..
				numutil.be16tostr(gotgroup) ..
				numutil.be16tostr(0x0000) ..
				numutil.be16tostr(0x0001) ..	-- group, not user
				numutil.be16tostr(0x0000) -- no TLVs
			})
		self:_updatemaster()
		newbuddyid = 1
	end
	self.groups[gotgroup].members[newbuddyid] = { name = account, friendly = afriendly, type = OSCAR.SSITypes.BUDDY }
	local tlv = ""
	if afriendly then
		tlv = OSCAR.TLV{type = 0x0131, value = afriendly}
	end
	self:debug("[BOS] [SSI] Adding user " .. account)
	-- Add the user
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0013, subtype = 0x0008, flags0 = 0, flags1 = 0, reqid = 0, data = 
			numutil.be16tostr(account:len()) .. account ..
			numutil.be16tostr(gotgroup) ..
			numutil.be16tostr(newbuddyid) ..
			numutil.be16tostr(OSCAR.SSITypes.BUDDY) ..
			numutil.be16tostr(tlv:len()) ..
			tlv
		})
	self:_updategroup(gotgroup)
	self:buddyadded(account, agroup, afriendly)
	
	self:debug("[BOS] [SSI] Ending transaction")
	-- End the transaction.
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0013, subtype = 0x0012, flags0 = 0, flags1 = 0, reqid = 0, data = 
			""
		})
	
	-- Activate new configuration
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0013, subtype = 0x0007, flags0 = 0, flags1 = 0, reqid = 0, data = 
			""
		})
	return 0
end

function OSCAR:im_remove_buddy(account, agroup)
	if self:comparenicks(account, ":RAW") == 0 then
		return 0
	end

	local gname
	
	self:debug("[BOS] [SSI] Remove buddy")
	if self.ssibusy then
		self:error("[BOS] [SSI] Attempt to remove buddy in transaction")
		return 1
	end
	-- Start a transaction first.
	self:debug("[BOS] [SSI] Transaction start")
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0013, subtype = 0x0011, flags0 = 0, flags1 = 0, reqid = 0, data = 
			""
		})
	for k,group in pairs(self.groups) do
		for k2,member in pairs(group.members) do
			if self:comparenicks(member.name, account) == 0 and (agroup:lower() == group.name:lower()) then
				self:debug("[BOS] [SSI] Removing "..member.name.." from group "..group.name)
				self:FLAPSend(
					OSCAR.SNAC:new{family = 0x0013, subtype = 0x000A, flags0 = 0, flags1 = 0, reqid = 0, data = 
						numutil.be16tostr(member.name:len()) .. member.name ..
						numutil.be16tostr(k) ..
						numutil.be16tostr(k2) ..
						numutil.be16tostr(0x0000) ..
						numutil.be16tostr(0x0000) -- no TLVs
					})
				group.members[k2] = nil
				self:_updategroup(k)
				-- No need to inform the UA -- it will take care of freeing the buddy object itself.
			end
		end

		if next(group.members) == nil and k ~= 0x0000 then	-- is the group empty? if so, delete that, too
			self:debug("[BOS] [SSI] Pruning empty group " .. group.name)
			self:FLAPSend(
				OSCAR.SNAC:new{family = 0x0013, subtype = 0x000A, flags0 = 0, flags1 = 0, reqid = 0, data = 
					numutil.be16tostr(group.name:len()) .. group.name ..
					numutil.be16tostr(k) ..
					numutil.be16tostr(0x0000) ..
					numutil.be16tostr(0x0001) ..	-- group, not user
					numutil.be16tostr(0x0000) -- no TLVs
				})
			self:_updatemaster()
		end
	end
	self:debug("[BOS] [SSI] Ending transaction")
	-- End the transaction.
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0013, subtype = 0x0012, flags0 = 0, flags1 = 0, reqid = 0, data = 
			""
		})
	
	-- Activate new configuration
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0013, subtype = 0x0007, flags0 = 0, flags1 = 0, reqid = 0, data = 
			""
		})
	return 0
end

-------------------------------------------------------------------------------
-- Administrative services
-------------------------------------------------------------------------------

function OSCAR:set_nickname(nick)
	if self:comparenicks(self.screenname, nick) ~= 0 then
		return naim.pd.fterrors.NOMATCH.num
	end
	
	self:FLAPSend(
		OSCAR.SNAC:new{family = 0x0007, subtype = 0x0004, flags0 = 0, flags1 = 0, reqid = 0, data =
			OSCAR.TLV{type = 0x0001, value=nick}
		})
	
	return 0
end

function OSCAR:BOSAccountModAck(snac)
	-- XXX: parse for error code?
	self:debug("[BOS] [Account Mod Ack]")
end

OSCAR.snacfamilydispatch[0x0007] = OSCAR.dispatchsubtype({
	[0x0005] = OSCAR.BOSAccountModAck,
	})

-------------------------------------------------------------------------------
-- Protocol driver stubs / required functions
-------------------------------------------------------------------------------
                                                                        
function OSCAR:comparenicks(a, b)
	if a:lower():gsub(" ","") == b:lower():gsub(" ","") then return 0 end
	return -1
end

function OSCAR:room_normalize(room)	-- cute! a failure in this function will cause naim to crash!
	room = room:lower()
	return room
end

-- All characters can be transmitted over the wire equally well.
function OSCAR:isprintable(c)
	return 0
end

-------------------------------------------------------------------------------
-- Socket management
-------------------------------------------------------------------------------

function OSCAR:preselect(r,w,e,n)
	if self.authsock then
		n = self.authsock:preselect(r,w,e,n)
	end
	
	for k,conn in pairs(self.flapconns or {}) do
		n = conn.sock:preselect(r,w,e,n)
	end
	return n
end

function OSCAR:cleanup(reason, verbose)
	if not reason then
		reason = naim.pd.fterrors.USERDISCONNECT.num
	elseif type(reason) == "table" then
		reason = reason.num
	end
	if not verbose then
		verbose = "unknown reason"
	end
	
	if self.authsock then
		self.authsock:close()
		self.authsock = nil
		self.authbuf = nil
	end
	
	for k,conn in pairs(self.flapconns) do
		conn.sock:close()
		conn.sock = nil
		conn.buf = nil
		self.flapconns[k] = nil
	end
	
	self.flapconns = nil
	self.flapspending = nil
	
	if self.isconnecting then
		self:connectfailed(reason, verbose)
	else
		naim.pd.internal.disconnect(self, reason)
	end
end

function OSCAR:postselect(r, w, e, n) self:wrap(function()
	if self.authsock then
		local err = self.authsock:postselect(r, w, e, self.authbuf)
		if err then
			self:cleanup(err, "(from authorizer)")
			return
		end
		if self.authsock and self.authsock:connected() and self.authbuf:readdata() and self.authbuf:pos() ~= 0 then
			self:got_data_authorizer()
		end
	end
	for k,conn in pairs(self.flapconns or {}) do
		local err = conn.sock:postselect(r, w, e, conn.buf)
		if err then
			if conn.required then
				-- This was fatal.
				self:cleanup(err, "(from BOS #"..k..")")
				return
			end
			
			-- Maybe the other end just timed us out from
			-- inactivity.  No sweat.
			conn.sock:close()
			conn.sock = nil
			conn.buf = nil
			self.flapconns[k] = nil
			
			self:notice("[FLAP] BOS #"..k.." disconnected; non-fatal.")
		end
		if conn.sock and conn.sock:connected() and conn.buf:readdata() and conn.buf:pos() ~= 0 then
			self:FLAPNewData(conn)
		end
	end
end) end

function OSCAR:connect(server, port, sn)
	if self.flapconns or self.authsock then
		self:error("Already connected!")
		return 1
	end
	if not server then server = "login.oscar.aol.com" end
	if port == 0 then port = 5190 end
	self:debug("Connecting to OSCAR on "..server.." port "..port..".")
	self.screenname = sn
	self.authsock = naim.socket.new()
	self.authbuf = naim.buffer.new()
	self.authbuf:resize(65550)
	self.authsock:connect(server, port)
	self.isconnecting = true
	self.icbm_req = 0
	self.icbm_recent = {}
	self.ssibusy = false
	self.flapconns = {}
	self.flapspending = {}
	
	return 0
end

function OSCAR:disconnect(err)
	self:cleanup(err)

	return 0
end

-------------------------------------------------------------------------------
-- PD registration
-------------------------------------------------------------------------------

function OSCAR:create(type)
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
	return o
end

if not OSCAR.loaded then
	naim.pd.create(OSCAR)
end
OSCAR.loaded = true
