function get_callback(http_status, buf, total_size, data_arg)
  assert(data_arg == "get_api", "unhandled callback?")
  print(http_status, buf, total_size)
end

wdb_pdwifi.init()

local apikey_file = playdate.file.open("apikey.txt")
local auth_token = apikey_file:readline()
apikey_file:close()

wdb_pdwifi.get("api.excelcoin.org",
               "/1.1/statuses/user_timeline.json?screen_name=zhuowei",
               "Authorization: " .. auth_token .. "\r\n",
               "get_callback", "get_api")

function playdate.update()
end
