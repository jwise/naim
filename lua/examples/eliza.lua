--  _ __   __ _ ___ __  __
-- | '_ \ / _` |_ _|  \/  | naim
-- | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
-- | | | | |_| || || |  | | Lua support Copyright 2006 Joshua Wise <joshua@joshuawise.com>
-- |_| |_|\__,_|___|_|  |_| ncurses-based chat client
--
-- eliza.lua
-- Psychiatrist module for naim


if naimpsych == nil then
	naimpsych = {}
end
naimpsych.conns = {}

function naim.commands.eliza(arg, conn)
  if arg == nil then
    naim.curconn():status_echo("[naimpsych] Syntax: /eliza &lt;nick&gt;")
    return
  end
  arg = string.lower(arg)
  arg = string.gsub(arg, " ", "")
  
  local cid=conn.id
  if naimpsych.conns[cid] == nil then
    naimpsych.conns[cid] = {}
  end
  if (naimpsych.conns[cid][arg] == nil) or (naimpsych.conns[cid][arg] == 0) then
    naimpsych.conns[cid][arg] = 1
    conn:msg(arg, [[
WELCOME TO THE NAIM PSYCHIATRIST. MY NAME IS ELIZA. YOU CAN SAY "BYE" AT ANY TIME TO MAKE ME GO AWAY.<br>
<br>
IT SURE IS NEAT TO HAVE YOU DROP BY. WHAT BRINGS YOU HERE?]])
    math.randomseed(os.time())
    conn:status_echo("[naimpsych] Starting psychiatrist session with " .. arg .. ". Type /eliza " .. arg .. " again to terminate.")
  else
    naimpsych.conns[cid][arg] = 0
    conn:status_echo("[naimpsych] Psychiatrist session with " .. arg .. " terminated.")
  end
  
end

function naimpsych.eliza(conn, sn, dest, text, flags)
  local response = ""
  text = string.gsub(text, "<[^>]*>", "")
  local user = string.upper(text)
  local userOrig = user
  local cid = conn.id
  sn = string.lower(sn)
  sn = string.gsub(sn, " ", "")
  
  if naimpsych.conns[cid] == nil or naimpsych.conns[cid][sn] == nil or naimpsych.conns[cid][sn] == 0  or dest ~= nil then
    return
  end

  -- randomly selected replies if no keywords
  local randReplies = {
    "WHAT DOES THAT SUGGEST TO YOU?",
    "I SEE...",
    "I'M NOT SURE I UNDERSTAND YOU FULLY.",
    "CAN YOU ELABORATE ON THAT?",
    "THAT IS QUITE INTERESTING!",
    "THAT'S SO... PLEASE CONTINUE...",
    "I UNDERSTAND...",
    "WELL, WELL... DO GO ON",
    "WHY ARE YOU SAYING THAT?",
    "PLEASE EXPLAIN THE BACKGROUND TO THAT REMARK...",
    "COULD YOU SAY THAT AGAIN, IN A DIFFERENT WAY?",
  }

  -- keywords, replies
  local replies = {
    [" CAN YOU"] = "PERHAPS YOU WOULD LIKE TO BE ABLE TO",
    [" DO YOU"] = "YES, I",
    [" CAN I"] = "PERHAPS YOU DON'T WANT TO BE ABLE TO",
    [" YOU ARE"] = "WHAT MAKES YOU THINK I AM",
    [" YOU'RE"] = "WHAT IS YOUR REACTION TO ME BEING",
    [" I DON'T"] = "WHY DON'T YOU",
    [" I FEEL"] = "TELL ME MORE ABOUT FEELING",
    [" WHY DON'T YOU"] = "WHY WOULD YOU WANT ME TO",
    [" WHY CAN'T I"] = "WHAT MAKES YOU THINK YOU SHOULD BE ABLE TO",
    [" ARE YOU"] = "WHY ARE YOU INTERESTED IN WHETHER OR NOT I AM",
    [" I CAN'T"] = "HOW DO YOU KNOW YOU CAN'T",
    [" SEX"] = "I FEEL YOU SHOULD DISCUSS THIS WITH A HUMAN.",
    [" I AM"] = "HOW LONG HAVE YOU BEEN",
    [" I'M"] = "WHY ARE YOU TELLING ME YOU'RE",
    [" I WANT"] = "WHY DO YOU WANT",
    [" WHAT"] = "WHAT DO YOU THINK?",
    [" HOW"] = "WHAT ANSWER WOULD PLEASE YOU THE MOST?",
    [" WHO"] = "HOW OFTEN DO YOU THINK OF SUCH QUESTIONS?",
    [" WHERE"] = "WHY DID YOU THINK OF THAT?",
    [" WHEN"] = "WHAT WOULD YOUR BEST FRIEND SAY TO THAT QUESTION?",
    [" WHY"] = "WHAT IS IT THAT YOU REALLY WANT TO KNOW?",
    [" PERHAPS"] = "YOU'RE NOT VERY FIRM ON THAT!",
    [" DRINK"] = "MODERATION IN ALL THINGS SHOULD BE THE RULE.",
    [" SORRY"] = "WHY ARE YOU APOLOGIZING?",
    [" DREAMS"] = "WHY DID YOU BRING UP THE SUBJECT OF DREAMS?",
    [" I LIKE"] = "IS IT GOOD THAT YOU LIKE",
    [" MAYBE"] = "AREN'T YOU BEING A BIT TENTATIVE?",
    [" NO"] = "WHY ARE YOU BEING NEGATIVE?",
    [" YOUR"] = "WHY ARE YOU CONCERNED ABOUT MY",
    [" ALWAYS"] = "CAN YOU THINK OF A SPECIFIC EXAMPLE?",
    [" THINK"] = "DO YOU DOUBT",
    [" YES"] = "YOU SEEM QUITE CERTAIN. WHY IS THIS SO?",
    [" FRIEND"] = "WHY DO YOU BRING UP THE SUBJECT OF FRIENDS?",
    [" COMPUTER"] = "WHY DO YOU MENTION COMPUTERS?",
    [" AM I"] = "YOU ARE",
  }

  -- conjugate
  local conjugate = {
    [" I "] = "YOU",
    [" ARE "] = "AM",
    [" WERE "] = "WAS",
    [" YOU "] = "ME",
    [" YOUR "] = "MY",
    [" I'VE "] = "YOU'VE",
    [" I'M "] = "YOU'RE",
    [" ME "] = "YOU",
    [" AM I "] = "YOU ARE",
    [" AM "] = "ARE",
  }

  -- random replies, no keyword
  local function replyRandomly()
    response = randReplies[math.random(table.getn(randReplies))].."\n"
  end

  -- find keyword, phrase
  local function processInput()
    for keyword, reply in pairs(replies) do
      local d, e = string.find(user, keyword, 1, 1)
      if d then
        -- process keywords
        response = response..reply.." "
        if string.byte(string.sub(reply, -1)) < 65 then -- "A"
          response = response.."\n"; return
        end
        local h = string.len(user) - (d + string.len(keyword))
        if h > 0 then
          user = string.sub(user, -h)
        end
        for cFrom, cTo in pairs(conjugate) do
          local f, g = string.find(user, cFrom, 1, 1)
          if f then
            local j = string.sub(user, 1, f - 1).." "..cTo
            local z = string.len(user) - (f - 1) - string.len(cTo)
            response = response..j.."\n"
            if z > 2 then
              local l = string.sub(user, -(z - 2))
              if not string.find(userOrig, l) then return end
            end
            if z > 2 then response = response..string.sub(user, -(z - 2)).."\n" end
            if z < 2 then response = response.."\n" end
            return
          end--if f
        end--for
        response = response..user.."\n"
        return
      end--if d
    end--for
    replyRandomly()
    return
  end

  -- main()
  -- accept user input
  if string.sub(user, 1, 3) == "BYE" then
    response = "BYE, BYE FOR NOW. SEE YOU AGAIN SOME TIME."
    naimpsych.conns[cid][sn] = 0
    conn:msg(sn, response)
    return
  end
  if string.sub(user, 1, 7) == "BECAUSE" then
    user = string.sub(user, 8)
  end
  user = " "..user.." "
  -- process input, print reply
  processInput()
  
  conn:msg(sn, response)
  return
end

if naimpsych.ref ~= nil then
	naim.hooks.recvfrom.del(naimpsych.ref)
end

naimpsych.ref = naim.hooks.recvfrom.add(naimpsych.eliza, 200)
naim.debug("[naimpsych] naim psychiatrist loaded. Type /eliza &lt;nick&gt; to use.")
