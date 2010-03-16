intstate  = 0
debug     = 0
rbuf      = ""
path      = ""
lhost     = ""
ltestport = ""
READSIZE = 256

function printdbg(str) 
   if (debug == 1) then
      print(str)
   end
end

function prepare(params, vname, vaddr, vport, vproto, vfwmark, rname, raddr, rport, rtestport)
   path = params
   lhost = raddr
   ltestport = rtestport
   printdbg(" ^^^ Params    = " .. params)
   printdbg(" ^^^ vname     = " .. vname)
   printdbg(" ^^^ vport     = " .. vport)
   printdbg(" ^^^ vproto    = " .. vproto)
   printdbg(" ^^^ vfwmark   = " .. vfwmark)
   printdbg(" ^^^ rname     = " .. rname)
   printdbg(" ^^^ raddr     = " .. raddr)
   printdbg(" ^^^ rport     = " .. rport)
   printdbg(" ^^^ rtestport = " .. rtestport)
end

function process_event(txt)
   local ret = 0
   local req = "DESCRIBE rtsp://" .. 
   	lhost ..
	":" ..
	ltestport ..
    	path .. 
	" RTSP/1.0\r\nCSeq: 1348\r\nAccept: application/sdp, application/rtsl, application/mheg\r\n\r\n"

   printdbg("rtsp.lua - process_event()")

   -- Switch to write
   if (intstate == 0) then
      printdbg("INTSTATE = 0");
      intstate = 1
      return "w", string.len(req), req
   end

   -- Switch to read
   if (intstate == 1) then
      printdbg("INTSTATE = 1")
      intstate = 2
      return "rav", READSIZE, ""
   end

   -- Switch to read
   if (intstate == 2) then
      printdbg("INTSTATE = 2")
      printdbg(txt)
      intstate = 3
      if (string.match(txt, "RTSP/1.0 200 OK")) then
	 printdbg(" !!! RTSP OK RECEIVED !!!")
	 return "eok", 0, ""
      else
        printdbg(" !!! RTSP NOT FOUND !!!")
	 return "efail", 0, ""
      end
   end
end

