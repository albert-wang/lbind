function add(a, b)
	print(a + b);
end

function sub(a, b)
	return a - b
end

function pt(t)
	for key,value in pairs(t) do print(key,value) end
end

pt(a)
a = 6.3;

foo:bar("Nyan", 2);

print(foo.a);
foo.a = 1;
foo:bar("!", 3);

print("Magic: " .. Foo.magic);