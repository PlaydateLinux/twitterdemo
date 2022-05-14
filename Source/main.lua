import "CoreLibs/graphics"

local tweets = nil

local kStatusLoading = 0
local kStatusError = 1
local kStatusSuccess = 2
local status = kStatusLoading

local errorString = ""

local apikey_file = playdate.file.open("apikey.txt")
local auth_token = apikey_file:readline()
apikey_file:close()

local username = "zhuowei"

function do_get()
  -- TODO(zhuowei): only one concurrent request is supported right now.
  -- the C api supports more, but I haven't implemented it yet
  -- must wait for get_callback to return.
  -- TODO(zhuowei): the get_callback isn't actually used right now...
  wdb_pdwifi.get("api.twitter.com",
                 "/1.1/statuses/user_timeline.json?screen_name=" .. username,
                 "Authorization: " .. auth_token .. "\r\n",
                 "get_callback", "get_api")
end

function do_poll()
  local http_status, buf, total_size, data_arg = wdb_pdwifi.poll()
  if data_arg == nil then
    return
  end
  assert(data_arg == "get_api", "unhandled callback?")

  print(http_status, buf, total_size)
  if http_status ~= 200 then
    status = kStatusError
    errorString = "Error: " .. http_status
    return
  end
  tweets = json.decode(buf)
  status = kStatusSuccess

end

local simulate = false

if simulate then
  tweets = json.decodeFile("demo_tweets.json")
  status = kStatusSuccess
else
  wdb_pdwifi.init()
  do_get()
end

function playdate.update()
  -- the C API's callbacks don't seem to be able to call Lua functions,
  -- so we instead just set a flag when it arrives, then call poll to run callbacks
  -- right now this only handles one at a time :(
  do_poll()
  playdate.graphics.clear()
  if status == kStatusLoading then
    playdate.graphics.drawText("loading...", 0, 0)
  elseif status == kStatusError then
    playdate.graphics.drawText(errorString, 0, 0)
  else
    playdate.graphics.drawTextInRect(tweets[1]["text"], 0, 0, playdate.display.getWidth(), 48)
  end
end
