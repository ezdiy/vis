-- complete word at primary selection location using vis-complete(1)

vis:map(vis.modes.INSERT, "<C-n>", function()
	local win = vis.win
	local file = win.file
	local pos = win.selection.pos
	if not pos then return end
	local range = file:text_object_word(pos > 0 and pos-1 or pos);
	if not range then return end
	if range.start == range.finish then return end
	local current = file:content(range)
	if not current then return end
	local prefix, suffix
	if range.finish > pos then
		prefix = current:sub(1, pos - range.start)
		suffix = current:sub(pos - range.start + 1)
	else
		prefix = current
		suffix = ""
	end
	local cmd = string.format("vis-complete --word '%s' '%s'", prefix:gsub("'", "'\\''"), suffix:gsub("'", "'\\''"))
	local status, out, err = vis:pipe(file, { start = 0, finish = file.size }, cmd)
	if status ~= 0 or not out then
		if err then vis:info(err) end
		return
	end
	file:insert(pos, out)
	win.selection.pos = pos + #out
end, "Complete word in file")
