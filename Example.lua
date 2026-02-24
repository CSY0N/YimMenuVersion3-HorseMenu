log.info("loaded successfully")
script.run_in_callback(function()
    while true do
      -- Code here

        script.yield(1000) -- sleep 1 second
    end
end)
