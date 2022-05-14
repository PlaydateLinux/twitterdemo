import "CoreLibs/graphics"
import "CoreLibs/ui"
import "CoreLibs/keyboard"

local tweets = nil

local kStatusLoading = 0
local kStatusError = 1
local kStatusSuccess = 2
local status = kStatusLoading

local errorString = ""

local apikey_file = playdate.file.open("apikey.txt")
local auth_token = apikey_file:readline()
apikey_file:close()

local username_file = playdate.file.open("username.txt")
local username = username_file:readline()
username_file:close()

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

  -- print(http_status, buf, total_size)
  if http_status ~= 200 then
    status = kStatusError
    errorString = "Error: " .. http_status
    return
  end
  tweets = json.decode(buf)
  status = kStatusSuccess
  repopulate_gridview()
end

local gridview = playdate.ui.gridview.new(0, 64)
gridview:setCellPadding(4, 4, 4, 4)
local gridScrollY = 0

function gridview:drawCell(section, row, column, selected, x, y, width, height)
  playdate.graphics.drawTextInRect(tweets[row]["text"], x, y, width, height, 0, "...")
end

function repopulate_gridview()
  gridview:setNumberOfRows(#tweets)
  gridScrollY = 0
  gridview:setScrollPosition(0, gridScrollY, false)
end

function playdate.keyboard.keyboardWillHideCallback(pressed_ok)
  if pressed_ok then
    username = playdate.keyboard.text
    local username_file = playdate.file.open("username.txt", playdate.file.kFileWrite)
    username_file:write(username .. "\n")
    username_file:close()
    status = kStatusLoading
    do_get()
  else
    status = kStatusSuccess
  end
end

local myInputHandlers = {
  AButtonDown = function()
    status = kStatusEnterName
    playdate.keyboard.show(username)
  end
}

playdate.inputHandlers.push(myInputHandlers)

local simulate = false

if simulate then
  tweets = json.decodeFile("demo_tweets.json")
  status = kStatusSuccess
  repopulate_gridview()
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
  elseif status == kStatusEnterName then
    playdate.graphics.drawText(playdate.keyboard.text, 0, 0)
  else
    gridScrollY += playdate.getCrankChange()
    gridview:setScrollPosition(0, gridScrollY, false)
    gridview:drawInRect(0, 0, playdate.display.getWidth(), playdate.display.getHeight())
  end
  playdate.timer:updateTimers()
end
