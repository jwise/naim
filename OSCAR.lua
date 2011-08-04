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

OSCAR.name = "OSCAR"		-- remember that this guy gets sourced and used as the PD itself!
OSCAR.server = "login.oscar.aol.com"
OSCAR.port = 5190
OSCAR.buffersize = 4096

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
	[OSCAR.NOTICE] = "[notice] ",
	[OSCAR.WARNING] = "[warning] ",
	[OSCAR.ERROR] = "[error] ",
	[OSCAR.FATAL] = "[fatal error] ",
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
	naim.echo("[OSCAR] "..OSCAR.echodescriptions[class]..text)
end
function OSCAR:debug(text) self:echo(OSCAR.DEBUG,text) end
function OSCAR:notice(text) self:echo(OSCAR.NOTICE,text) end
function OSCAR:warning(text) self:echo(OSCAR.WARNING,text) end
function OSCAR:error(text) self:echo(OSCAR.ERROR,text) end
function OSCAR:fatal(text)
	self:echo(OSCAR.FATAL,text)
	self:cleanup(naim.pd.fterrors.WEIRDPACKET, text)
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
					self.authcookie = authcookie
					
					self:debug("[auth] connecting to BOS on "..bosaddr.." port "..bosport.. " as "..self.screenname)
					
					self.bossock = naim.socket.new()
					self.bosbuf = naim.buffer.new()
					self.bosbuf:resize(65550)
					self.bossock:connect(bosaddr, bosport)
					
					if bufdata:len() ~= 0 then
						self:error("[auth] there was still data left over from the authorizer when I killed its buffer...")
					end
					self.authsock:close()
					self.authsock = nil
					self.authbuf = nil
					return 0
				end
			end
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

function OSCAR:got_data_bos()
	local bufdata = self.bosbuf:peek()
	while bufdata and OSCAR.FLAP:lengthfromstring(bufdata) and OSCAR.FLAP:lengthfromstring(bufdata) <= bufdata:len() do
		local len = OSCAR.FLAP:lengthfromstring(bufdata)
		local flap = OSCAR.FLAP:fromstring(self.bosbuf:take(len))
		bufdata = self.bosbuf:peek()
		
		-- now we can dispatch the FLAP
		if flap.channel == 1 then
			-- hopefully it is a Connection Acknowledge
			if flap.data ~= string.char(0x00, 0x00, 0x00, 0x01) then
				return self:fatal("[BOS] That wasn't a connection acknowledge!")
			end
			-- okay, it is; let's send the auth request
			self.bossock:send(
				OSCAR.FLAP:new({ channel = 1, seq = 0, data =
					string.char(0x00, 0x00, 0x00, 0x01) ..
					self.TLV{type = 0x0006, value = self.authcookie}
				}):tostring()
				)
			self.bosseq = 0
		elseif flap.channel == 2 then	-- data SNACs
			local snac = OSCAR.SNAC:fromstring(flap.data)
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
			self.bossock:close()
			self.bossock = nil
			self.bosbuf = nil
		else
			self:error("[BOS] Unknown channel: "..flap.channel)
		end
	end
	return 0
end

-------------------------------------------------------------------------------
-- BOS Service Control SNACs (family 0x0001)
-------------------------------------------------------------------------------

OSCAR.Errors = { "Invalid SNAC header", "Server rate limit exceeded", "Client rate limit exceeded", "Recipient is not logged in",
		"Requested service unavailable", "Requested service not defined", "Obsolete SNAC", "Not supported by server",
		"Not supported by client", "Refused by client", "Reply too big", "Responses lost",
		"Request denied", "Incorrect SNAC format", "Insufficient rights", "Recipient blocked",
		"Sender too evil", "Receiver too evil", "User temporarily unavailable", "No match",
		"List overflow", "Request ambiguous", "Server queue full", "Not while on AOL (?)" }
	
function OSCAR:BOSControlError(snac)
	if OSCAR.Errors[numutil.strtobe16(snac.data)] then
		self:error("[BOS] [Error] Server reports " .. OSCAR.Errors[numutil.strtobe16(snac.data)] .. " (".. numutil.tohex(snac.data)..")")
	else
		self:error("[BOS] [Error] Unknown server error " .. numutil.tohex(snac.data))
	end
end

function OSCAR:BOSControlHostReady(snac)	
	local families = 0
	self.families = {}
	while snac.data:len() > 1 do
		self.families[numutil.strtobe16(snac.data)] = true
		families = families + 1
		snac.data = snac.data:sub(3)
	end
	self:debug("[BOS] [Host Ready] Sending rate request")
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0001, subtype = 0x0006, flags0 = 0, flags1 = 0, reqid = 0, data = ""}
				):tostring()
			}):tostring())
end

function OSCAR:BOSControlRateResponse(snac)
	local ratecount = snac.data:byte(1) * 256 + snac.data:byte(2)
	self:debug("[BOS] [Rate Response] Got "..ratecount.." rates")
	--local acceptedrates = ""
	--for i=1,ratecount do
	--	acceptedrates = acceptedrates .. snac.data:sub(3+(i-1)*35, 4+(i-1)*35)
	--end
	acceptedrates = string.char(0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05)
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0001, subtype = 0x0008, flags0 = 0, flags1 = 0, reqid = 1, data = 
				acceptedrates
				}):tostring()
			}):tostring())
	self:debug("[BOS] [Rate Response] Sent rate acknowledge SNAC")
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0001, subtype = 0x0014, flags0 = 0, flags1 = 0, reqid = 2, data = 
				string.char(0x00, 0x00, 0x00, 0x03)
				}):tostring()
			}):tostring())
	self:debug("[BOS] [Rate Response] Sent privacy SNAC")
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0001, subtype = 0x000e, flags0 = 0, flags1 = 0, reqid = 3, data = 
				""
				}):tostring()
			}):tostring())
	self:debug("[BOS] [Rate Response] Sent user info request SNAC")
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0002, subtype = 0x0002, flags0 = 0, flags1 = 0, reqid = 4, data = 
				""
				}):tostring()
			}):tostring())
	self:debug("[BOS] [Rate Response] Sent location limit request SNAC")
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0003, subtype = 0x0002, flags0 = 0, flags1 = 0, reqid = 5, data = 
				""
				}):tostring()
			}):tostring())
	self:debug("[BOS] [Rate Response] Sent blist limit request SNAC")
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0004, subtype = 0x0004, flags0 = 0, flags1 = 0, reqid = 6, data = 
				""
				}):tostring()
			}):tostring())
	self:debug("[BOS] [Rate Response] Sent ICBM param request SNAC")
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0009, subtype = 0x0002, flags0 = 0, flags1 = 0, reqid = 7, data = 
				""
				}):tostring()
			}):tostring())
	self:debug("[BOS] [Rate Response] Sent privacy param request SNAC")
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0013, subtype = 0x0002, flags0 = 0, flags1 = 0, reqid = 8, data = 
				""
				}):tostring()
			}):tostring())
	self:debug("[BOS] [Rate Response] Sent SSI param request SNAC")
end

function OSCAR:BOSControlURL(snac)
	self:notice("[BOS] [URL] URL: " .. snac.data)
end

function OSCAR:BOSControlOnlineInfo(snac)
	self:debug("[BOS] [BOS Online Info] Weh.")
end

OSCAR.snacfamilydispatch[0x0001] = OSCAR.dispatchsubtype({
	[0x0001] = OSCAR.BOSControlError,
	[0x0003] = OSCAR.BOSControlHostReady,
	[0x0007] = OSCAR.BOSControlRateResponse,
	[0x000F] = OSCAR.BOSControlOnlineInfo,
	[0x0015] = OSCAR.BOSControlURL,
	})

-------------------------------------------------------------------------------
-- Location control SNACs (family 0x0002)
-------------------------------------------------------------------------------

function OSCAR:BOSLocationError(snac)
	if OSCAR.Errors[numutil.strtobe16(snac.data)] then
		self:error("[BOS] [Location Error] Server reports " .. OSCAR.Errors[numutil.strtobe16(snac.data)] .. " (".. numutil.tohex(snac.data)..")")
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
	self:debug("[BOS] [Location Limitations] Sent profile et al")
end

function OSCAR:BOSLocationGotInfo(snac)
	local screenname, wlevel, tlvcount = "", 0, 0
	local away, info, online, idle, flags = nil, "", 0, 0, 0
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
		    if tlv.type == 0x0001 then flags = numutil.strtobe16(tlv.value)
		elseif tlv.type == 0x0004 and tlv.value:len() == 2 then idle = numutil.strtobe16(tlv.value)
		elseif tlv.type == 0x0004 and tlv.value:len() ~= 2 then away = tlv.value
		elseif tlv.type == 0x0003 and tlv.value:len() == 4 then online = numutil.strtobe16(tlv.value)*65536+numutil.strtobe16(tlv.value:sub(3))
		elseif tlv.type == 0x0002 then info = tlv.value
		end
	end
	if snac.data:len() > 0 then
		self:debug(tohex(snac.data))
	end
	if away and away:len() ~= 0 then
		info = "<b>Away message:</b><br>"..away.."<br><hr><b>Profile:</b><br>"..info
	end
	
	self:gotinfo(screenname, info, wlevel, online, idle, flags)
end

OSCAR.snacfamilydispatch[0x0002] = OSCAR.dispatchsubtype({
	[0x0001] = OSCAR.BOSLocationError,
	[0x0003] = OSCAR.BOSLocationLimitations,
	[0x0006] = OSCAR.BOSLocationGotInfo,
	})

function OSCAR:set_info(info)
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0002, subtype = 0x0004, flags0 = 0, flags1 = 0, reqid = 0, data = 
				OSCAR.TLV{type = 0x0001, value='text/x-aolrtf; charset="us-ascii"'} ..
				OSCAR.TLV{type = 0x0002, value=info}
				}):tostring()
			}):tostring())
end

function OSCAR:set_away(away, isauto)
	if not away then away = "" end
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0002, subtype = 0x0004, flags0 = 0, flags1 = 0, reqid = 0, data = 
				OSCAR.TLV{type = 0x0003, value='text/x-aolrtf; charset="us-ascii"'} ..
				OSCAR.TLV{type = 0x0004, value=away}
				}):tostring()
			}):tostring())
	return 0
end

function OSCAR:get_info(user)
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0002, subtype = 0x0015, flags0 = 0, flags1 = 0, reqid = 0, data = 
				string.char(0x00, 0x00, 0x00, 0x07, user:len()) .. user
				}):tostring()
			}):tostring())
	return 0
end

-------------------------------------------------------------------------------
-- Buddy list SNACs (family 0x0003)
-------------------------------------------------------------------------------

function OSCAR:BOSBlistError(snac)
	if OSCAR.Errors[numutil.strtobe16(snac.data)] then
		self:error("[BOS] [Blist Error] Server reports " .. OSCAR.Errors[numutil.strtobe16(snac.data)] .. " (".. numutil.tohex(snac.data)..")")
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
	-- pass this back up to naim somehow?
end

function OSCAR:BOSBlistOnline(snac)
	local uname
	uname = snac.data:sub(2, snac.data:byte(1)+1)
	self:debug("[BOS] [Blist online] " .. uname)
	self:im_buddyonline(uname, 1)
end

function OSCAR:BOSBlistOffline(snac)
	local uname
	uname = snac.data:sub(2, snac.data:byte(1)+1)
	self:debug("[BOS] [Blist offline] " .. uname)
	self:im_buddyonline(uname, 0)
end

OSCAR.snacfamilydispatch[0x0003] = OSCAR.dispatchsubtype({
	[0x0001] = OSCAR.BOSBlistError,
	[0x0003] = OSCAR.BOSBlistParameters,
	[0x000A] = OSCAR.BOSBlistAddFailed,
	[0x000B] = OSCAR.BOSBlistOnline,
	[0x000C] = OSCAR.BOSBlistOffline,
	})

-------------------------------------------------------------------------------
-- ICBM SNACs (family 0x0004)
-------------------------------------------------------------------------------

function OSCAR:BOSICBMParameters(snac)
	self:debug("[BOS] [ICBM Parameters Reply] Undecoded; sending set ICBM request")
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0004, subtype = 0x0002, flags0 = 0, flags1 = 0, reqid = 0, data = 
				string.char(	0x00, 0x00, -- channel
						0x00, 0x00, 0x00, 0x07, -- message flags
						0x1F, 0x40, -- max message snac size
						0x03, 0xE7, -- max sender warning level
						0x03, 0xE7, -- max rcvr warning level
						0x00, 0x00, 0x00, 0x00)
				}):tostring()
			}):tostring())
	self:debug("[BOS] [ICBM Parameters Reply] Finalizing.")
	self:doinit(self.screenname)
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0001, subtype = 0x001E, flags0 = 0, flags1 = 0, reqid = 0, data = 
				OSCAR.TLV{type = 0x0006, value=string.char(0x00, 0x00, 0x00, 0x00)}
				}):tostring()
			}):tostring())
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0001, subtype = 0x0002, flags0 = 0, flags1 = 0, reqid = 0, data = 
				string.char(0x00,0x15,0x00,0x01,0x01,0x10,0x04,0x7c) ..
				string.char(0x00,0x13,0x00,0x04,0x01,0x10,0x06,0x29) ..
				string.char(0x00,0x0c,0x00,0x01,0x01,0x04,0x00,0x21) ..
				string.char(0x00,0x0b,0x00,0x01,0x01,0x04,0x00,0x21) ..
				string.char(0x00,0x0a,0x00,0x01,0x01,0x10,0x06,0x29) ..
				string.char(0x00,0x09,0x00,0x01,0x01,0x10,0x06,0x29) ..
				string.char(0x00,0x08,0x00,0x01,0x01,0x01,0x04,0x00) ..
				string.char(0x00,0x04,0x00,0x01,0x01,0x10,0x06,0x29) ..
				string.char(0x00,0x03,0x00,0x01,0x01,0x10,0x06,0x29) ..
				string.char(0x00,0x02,0x00,0x01,0x01,0x10,0x06,0x29) ..
				string.char(0x00,0x01,0x00,0x01,0x01,0x10,0x06,0x29) ..
				""
				}):tostring()
			}):tostring())
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
		    if tlv.type == 0x0101 then realmessage = tlv.value:sub(5)
		end
	end
	if message:len() > 0 then
		self:error("[BOS] [ICBM] WARNING: Data left over in the TLV TLV! "..OSCAR.tohex(message))
		self:error("[BOS] [ICBM] remaining bytes: " .. message:len() .. " needed bytes: " .. OSCAR.TLV:lengthfromstring(message))
	end
	
	if realmessage:sub(1,4):lower() == "/me " then
		self:im_getaction(screenname, auto, realmessage:sub(5))
	elseif realmessage:match(">/[mM][eE] ") then
		self:im_getaction(screenname, auto, realmessage:match(">/[mM][eE] (.*)"))
	else
		self:im_getmessage(screenname, auto, realmessage)
	end
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
	[0x0005] = OSCAR.BOSICBMParameters,
	[0x0007] = OSCAR.BOSICBM,
	[0x000A] = OSCAR.BOSICBMMissed,
	[0x0014] = OSCAR.BOSICBMTypingNotification,
	})

function OSCAR:im_send_message(target, text, isauto)
	local isautotlv = ""
	if target == ":OSCAR" then
		text = text:gsub("<.->",""):lower()
		if text == "request-ssi" then
			self:warning("Rerequesting SSIs")
			self:BOSSSILimitReply(nil)
		end
		self:warning(":OSCAR handler execution stopped")
		return 0
	end
	if isauto == 1 then
		isautotlv = OSCAR.TLV{type = 0x0004, value = ""}
	end
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0004, subtype = 0x0006, flags0 = 0, flags1 = 0, reqid = 0, data = 
				string.char(	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07, -- cookie
						0x00,0x01, -- channel
						target:len()) .. target ..
				OSCAR.TLV{type = 0x0002, value = 
					string.char(0x05,0x01,0x00,0x01,0x01,0x01,0x01)..
					numutil.be16tostr(text:len()+4)..
					string.char(0x00,0x00,0x00,0x00)..
					text} ..
				isautotlv
				}):tostring()
			}):tostring())
	return 0
end

function OSCAR:im_send_action(target, text, isauto)
	return self:im_send_message(target, "/me "..text, isauto)
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
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0013, subtype = 0x0004, flags0 = 0, flags1 = 0, reqid = 0, data = 
				""
				}):tostring()
			}):tostring())
	self.groups = {}
	self.buddies = {}
end

function OSCAR:_snactorosterentry(indata)
	local itemname, groupid, itemid, type, data, nickname, buddycomment, contents
	
	itemname = indata:sub(3, numutil.strtobe16(indata) + 2)
	indata = indata:sub(numutil.strtobe16(indata) + 3)
	groupid = numutil.strtobe16(indata)
	indata = indata:sub(3)
	itemid = numutil.strtobe16(indata)
	indata = indata:sub(3)
	type = numutil.strtobe16(indata)
	indata = indata:sub(3)
	data = indata:sub(3, numutil.strtobe16(indata) + 2)
	indata = indata:sub(numutil.strtobe16(indata) + 3)
	
	local nickname, buddycomment, contents
	while data and OSCAR.TLV:lengthfromstring(data) and OSCAR.TLV:lengthfromstring(data) <= data:len() do
		local tlv = OSCAR.TLV(data)
		data = data:sub(tlv.value:len() + 5)
		    if tlv.type == 0x0131 then nickname = tlv.value
		elseif tlv.type == 0x013C then buddycomment = tlv.value
		elseif tlv.type == 0x00C8 then contents = tlv.value
		end
	end
	
	local types = { [0x0000] = "Buddy record", [0x0001] = "Group record", [0x0002] = "Permit record",
			[0x0003] = "Deny record", [0x0004] = "Permit/deny settings", [0x0005] = "Presence info",
			[0x000E] = "Ignore list record", [0x000F] = "Last update date", [0x0019] = "Buddy record (deleted)"}
	local thestring = "[BOS] [SSI] Item: " .. itemname .. ", group " .. groupid .. ", item " ..itemid .. ", "
	if types[type] then thestring = thestring .. types[type]
	else thestring = thestring .. "Type " .. type
	end
	thestring = thestring .. ", "
	if data:len() > 0 then thestring = thestring .. "extra data!, " end
	if nickname then thestring = thestring .. "nickname " .. nickname .. ", " end
	if buddycomment then thestring = thestring .. "buddycomment " .. buddycomment .. ", " end
	if contents then thestring = thestring .. "contents, " end
	self:debug(thestring .. "end.")
	
	return indata, itemname, groupid, itemid, type, data, nickname, buddycomment, contents
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
	
	for i = 1,items do
		local itemname, groupid, itemid, type, data, nickname, buddycomment, contents
		
		snac.data, itemname, groupid, itemid, type, data, nickname, buddycomment, contents = self:_snactorosterentry(snac.data)
		
		if type == 0x0001 then
			if not self.groups[groupid] then
				self.groups[groupid] = {}
			end
			self.groups[groupid].members = {}
			if groupid ~= 0 then
				self.groups[groupid].name = itemname
				while contents:len() > 1 do
					local buddy
					buddy = numutil.strtobe16(contents)
					contents = contents:sub(3)
					self.groups[groupid].members[buddy] = {}
				end
			else
				self.groups[groupid].name = "Unassigned"
			end
		elseif type == 0x0000 then
			if not self.groups[groupid] then
				self:error("[BOS] [SSI] Item without group created first?")
				self.groups[groupid] = {}
				self.groups[groupid].members = {}
			end
			self.groups[groupid].members[itemid].name = itemname
			self.groups[groupid].members[itemid].friendly = nickname
			self.groups[groupid].members[itemid].comment = comment
		end
	end
	if naim.bit._and(snac.flags1, 1) == 0 then
		-- all done
		for k,group in pairs(self.groups) do
			if not group.name then
				group.name = "Unknown group"
			end
			self:debug("[BOS] [SSI] ["..group.name.."]")
			for k2,v in pairs(group.members) do
				if v.friendly then
					self:debug("[BOS] [SSI] * ".. v.name .. " ("..v.friendly..")")
				elseif v.name then
					self:debug("[BOS] [SSI] * ".. v.name)
				else
					self:debug("[BOS] [SSI] *** UNKNOWN ***")
				end
				if v.name then
					self:buddyadded(v.name, group.name, v.friendly)
				end
			end
		end
		-- make it so! send the activate SSI stuff down the wire
		self.bossock:send(
			OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
				OSCAR.SNAC:new({family = 0x0013, subtype = 0x0007, flags0 = 0, flags1 = 0, reqid = 0, data = 
					""
					}):tostring()
				}):tostring())
	end
end

function OSCAR:BOSSSIRemoveItem(snac)
	if not self.ssibusy then
		self:fatal("[BOS] [SSI Remove Item] Item removed outside of transaction ... ")
	end
	self:debug("[BOS] [SSI Remove Item]")
	
	local itemname, groupid, itemid, type, data, nickname, buddycomment, contents
		
	snac.data, itemname, groupid, itemid, type, data, nickname, buddycomment, contents = self:_snactorosterentry(snac.data)
	
	if type == 0x0000 then
		if not self.groups[groupid] then
			self:error("[BOS] [SSI Remove Item] That group ID doesn't exist ...")
		end
		self.groups[groupid].members[itemid] = nil
	elseif type == 0x0001 then
		self.groups[groupid] = nil
	end
	if snac.data:len() ~= 0 then
		self:fatal("[BOS] [SSI Remove Item] data left over ...")
	end
	
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0013, subtype = 0x0007, flags0 = 0, flags1 = 0, reqid = 0, data = 
				""
				}):tostring()
			}):tostring())
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
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0013, subtype = 0x0007, flags0 = 0, flags1 = 0, reqid = 0, data = 
				""
				}):tostring()
			}):tostring())
end

OSCAR.snacfamilydispatch[0x0013] = OSCAR.dispatchsubtype({
	[0x0003] = OSCAR.BOSSSILimitReply,
	[0x0006] = OSCAR.BOSSSIRosterReply,
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
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0013, subtype = 0x0009, flags0 = 0, flags1 = 0, reqid = 0, data = 
				numutil.be16tostr(self.groups[id].name:len()) .. self.groups[id].name ..
				numutil.be16tostr(id) ..
				numutil.be16tostr(0x0000) ..
				numutil.be16tostr(0x0001) ..
				numutil.be16tostr(tlvs:len())  ..
				tlvs
				}):tostring()
			}):tostring())
end

function OSCAR:_updatemaster()
	local tlvdata = ""
	local tlvs = ""
	
	self:debug("[BOS] [SSI] Master update")
	for k,member in pairs(self.groups) do
		tlvdata = tlvdata .. numutil.be16tostr(k)
	end
	tlvs = OSCAR.TLV{type = 0x00c8, value = tlvdata}
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0013, subtype = 0x0009, flags0 = 0, flags1 = 0, reqid = 0, data = 
				numutil.be16tostr(0x0000) .. 
				numutil.be16tostr(0x0000) ..
				numutil.be16tostr(0x0000) ..
				numutil.be16tostr(0x0001) ..
				numutil.be16tostr(tlvs:len())  ..
				tlvs
				}):tostring()
			}):tostring())
end

function OSCAR:im_add_buddy(account, agroup, afriendly)
	if self.ssibusy then
		self:error("[BOS] [SSI] Attempt to add buddy in transaction")
		return 1
	end
	self:debug("[BOS] [SSI] Add buddy")
	-- Start a transaction first.
	self:debug("[BOS] [SSI] Transaction start")
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0013, subtype = 0x0011, flags0 = 0, flags1 = 0, reqid = 0, data = 
				""
				}):tostring()
			}):tostring())
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
				self.bossock:send(
					OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
						OSCAR.SNAC:new({family = 0x0013, subtype = 0x000A, flags0 = 0, flags1 = 0, reqid = 0, data = 
							numutil.be16tostr(member.name:len()) .. member.name ..
							numutil.be16tostr(k) ..
							numutil.be16tostr(k2) ..
							numutil.be16tostr(0x0000) ..
							numutil.be16tostr(0x0000) -- no TLVs
							}):tostring()
						}):tostring())
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
			self.bossock:send(
				OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
					OSCAR.SNAC:new({family = 0x0013, subtype = 0x000A, flags0 = 0, flags1 = 0, reqid = 0, data = 
						numutil.be16tostr(group.name:len()) .. group.name ..
						numutil.be16tostr(k) ..
						numutil.be16tostr(0x0000) ..
						numutil.be16tostr(0x0001) ..	-- group, not user
						numutil.be16tostr(0x0000) -- no TLVs
						}):tostring()
					}):tostring())
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
		self.bossock:send(
			OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
				OSCAR.SNAC:new({family = 0x0013, subtype = 0x0008, flags0 = 0, flags1 = 0, reqid = 0, data = 
					numutil.be16tostr(agroup:len()) .. agroup ..
					numutil.be16tostr(gotgroup) ..
					numutil.be16tostr(0x0000) ..
					numutil.be16tostr(0x0001) ..	-- group, not user
					numutil.be16tostr(0x0000) -- no TLVs
					}):tostring()
				}):tostring())	
		self:_updatemaster()
		newbuddyid = 1
	end
	self.groups[gotgroup].members[newbuddyid] = { name = account, friendly = afriendly }
	local tlv = ""
	if afriendly then
		tlv = OSCAR.TLV{type = 0x0131, value = afriendly}
	end
	self:debug("[BOS] [SSI] Adding user " .. account)
	-- Add the user
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0013, subtype = 0x0008, flags0 = 0, flags1 = 0, reqid = 0, data = 
				numutil.be16tostr(account:len()) .. account ..
				numutil.be16tostr(gotgroup) ..
				numutil.be16tostr(newbuddyid) ..
				numutil.be16tostr(0x0000) ..
				numutil.be16tostr(tlv:len()) ..
				tlv
				}):tostring()
			}):tostring())	
	self:_updategroup(gotgroup)
	self:buddyadded(account, agroup, afriendly)
	
	self:debug("[BOS] [SSI] Ending transaction")
	-- End the transaction.
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0013, subtype = 0x0012, flags0 = 0, flags1 = 0, reqid = 0, data = 
				""
				}):tostring()
			}):tostring())
	
	-- Activate new configuration
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0013, subtype = 0x0007, flags0 = 0, flags1 = 0, reqid = 0, data = 
				""
				}):tostring()
			}):tostring())
	return 0
end

function OSCAR:im_remove_buddy(account, agroup)
	local gname
	
	self:debug("[BOS] [SSI] Remove buddy")
	if self.ssibusy then
		self:error("[BOS] [SSI] Attempt to remove buddy in transaction")
		return 1
	end
	-- Start a transaction first.
	self:debug("[BOS] [SSI] Transaction start")
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0013, subtype = 0x0011, flags0 = 0, flags1 = 0, reqid = 0, data = 
				""
				}):tostring()
			}):tostring())
	for k,group in pairs(self.groups) do
		for k2,member in pairs(group.members) do
			if self:comparenicks(member.name, account) == 0 and (agroup:lower() == group.name:lower()) then
				self:debug("[BOS] [SSI] Removing "..member.name.." from group "..group.name)
				self.bossock:send(
					OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
						OSCAR.SNAC:new({family = 0x0013, subtype = 0x000A, flags0 = 0, flags1 = 0, reqid = 0, data = 
							numutil.be16tostr(member.name:len()) .. member.name ..
							numutil.be16tostr(k) ..
							numutil.be16tostr(k2) ..
							numutil.be16tostr(0x0000) ..
							numutil.be16tostr(0x0000) -- no TLVs
							}):tostring()
						}):tostring())
				group.members[k2] = nil
				self:_updategroup(k)
				self:buddyremoved(member.name, group.name)
			end
		end

		if next(group.members) == nil and k ~= 0x0000 then	-- is the group empty? if so, delete that, too
			self:debug("[BOS] [SSI] Pruning empty group " .. group.name)
			self.bossock:send(
				OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
					OSCAR.SNAC:new({family = 0x0013, subtype = 0x000A, flags0 = 0, flags1 = 0, reqid = 0, data = 
						numutil.be16tostr(group.name:len()) .. group.name ..
						numutil.be16tostr(k) ..
						numutil.be16tostr(0x0000) ..
						numutil.be16tostr(0x0001) ..	-- group, not user
						numutil.be16tostr(0x0000) -- no TLVs
						}):tostring()
					}):tostring())
			self:_updatemaster()
		end
	end
	self:debug("[BOS] [SSI] Ending transaction")
	-- End the transaction.
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0013, subtype = 0x0012, flags0 = 0, flags1 = 0, reqid = 0, data = 
				""
				}):tostring()
			}):tostring())
	
	-- Activate new configuration
	self.bossock:send(
		OSCAR.FLAP:new({ channel = 2, seq = self:nextbosseq(), data =
			OSCAR.SNAC:new({family = 0x0013, subtype = 0x0007, flags0 = 0, flags1 = 0, reqid = 0, data = 
				""
				}):tostring()
			}):tostring())
	return 0
end

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

-------------------------------------------------------------------------------
-- Connection management
-------------------------------------------------------------------------------

function OSCAR:preselect(r, w, e, n)
end

function OSCAR:preselect_hook(r,w,e,n)
	if self.authsock then
		n = self.authsock:preselect(r,w,e,n)
	end
	if self.bossock then
		n = self.bossock:preselect(r,w,e,n)
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
	
	if self.bossock then
		self.bossock:close()
		self.bossock = nil
		self.bosbuf = nil
	end
	
	if self.isconnecting then
		self:connectfailed(reason, verbose)
	else
		self:disconnect(reason)
	end
end

function OSCAR:postselect(r, w, e, n)
	if self.authsock then
		local err = self.authsock:postselect(r, w, e, self.authbuf)
		if err then
			self:fatal("Postselect error on authorizer socket: " .. naim.strerror(err))
			self:cleanup(err, "(from authorizer)")
		end
		if self.authsock and self.authsock:connected() and self.authbuf:readdata() and self.authbuf:pos() ~= 0 then
			self:got_data_authorizer()
		end
	end
	if self.bossock and self.bosbuf then
		local err = self.bossock:postselect(r, w, e, self.bosbuf)
		if err then
			self:fatal("Postselect error on BOS socket: " .. naim.strerror(err))
			self:cleanup(err, "(from BOS)")
		end
		if self.bossock:connected() and self.bosbuf:readdata() and self.bosbuf:pos() ~= 0 then
			self:got_data_bos()
		end
	end
end

function OSCAR:connect(server, port, sn)
	if self.bossock or self.authsock then
		self:error("Already connected!")
		return 1
	end
	if not server then server = "login.oscar.aol.com" end
	if port == 0 then port = 5190 end
	self.screenname = sn
	self.authsock = naim.socket.new()
	self.authbuf = naim.buffer.new()
	self.authbuf:resize(65550)
	self.authsock:connect(server, port)
	self.isconnecting = true
	-- work around hook bug that I can't be arsed to fix
	self.preselect_hook_ref = naim.hooks.add('preselect', function(r,w,e,n) return true,self:preselect_hook(r,w,e,n) end, 100)
	
	return 0
end

function OSCAR:disconnect()
	if self.bossock then
		self.bossock:close()
	end
	if self.authsock then
		self.authsock:close()
	end
	self.authsock = nil
	self.authbuf = nil
	self.bossock = nil
	self.bosbuf = nil
	self.groups = nil
	self.buddies = nil
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
