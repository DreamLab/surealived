intstate = 0
debug    = 0
rbuf     = ""
username = ""
password = ""
READSIZE = 256

function printdbg(str) 
   if (debug == 1) then
      print(str)
   end
end

function prepare(params)
   printdbg(" ^^^ Params = " .. params)
   username, password = string.match(params, "username=([%a%d]+); password=([%@%a%d]+)")
   printdbg(" ^^^ parsed params [username, password] = [" .. username .. ", " .. password .. "]")
   username = "USER " .. username .. "\r\n"
   password = "PASS " .. password .. "\r\n"
--   username = "USER anonymous\r\n"
--   password = "PASS test@\r\n"
end

function process_event(txt)
   local ret = 0

   printdbg("sunsite.lua - process_event()")

   -- Switch to read
   if (intstate == 0) then
      printdbg("INTSTATE = 0");
      intstate = 1
      return "rav", READSIZE, ""
   end

   -- Switch to write
   if (intstate == 1) then
      printdbg("INTSTATE = 1")
      rbuf = rbuf .. txt
      printdbg("READ 1:\n" .. rbuf)
      printdbg("======================================")
      if (string.match(rbuf, " ready.")) then
	 printdbg(" !!! MATCH ( ready.) !!!")
	 intstate = 2
	 rbuf = ""
	 return "w", string.len(username), username
      end
      return "rav", READSIZE, ""
   end

   -- Switch to read
   if (intstate == 2) then
      printdbg("INTSTATE = 2")
      rbuf = ""
      intstate = 3
      return "rav", READSIZE, ""
   end

   -- Switch to write
   if (intstate == 3) then
      printdbg("INTSTATE = 3")
      rbuf = rbuf .. txt
      printdbg("READ 3:\n" .. rbuf)
      intstate = 4
      return "w", string.len(password), password
   end

   -- Switch to read
   if (intstate == 4) then
      printdbg("INTSTATE = 4")
      rbuf = ""
      intstate = 5
      return "rav", READSIZE, ""
   end

   if (intstate == 5) then
      printdbg("INTSTATE = 5")
      rbuf = rbuf .. txt
      printdbg("READ 5:\n" .. rbuf)
      if (string.match(rbuf, "230 Guest login ok")) then
	 printdbg(" !!! 230 Guest login ok !!!")
	 return "eok", 0, ""
      end

      if (string.match(rbuf, "530 Login incorrect")) then
	 printdbg(" !!! 530 Login incorrect !!!")
	 return "efail", 0, ""
      end
      
      return "rav", READSIZE, ""
   end

   return "efail", 0, ""
end

