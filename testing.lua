function add(a, b)
	print(a + b);
end

function sub(a, b)
	return a - b
end

function pt(t)
	for key,value in pairs(t) do print(key,value) end
end


print(foo:bar(foo))
print(foo.a);
foo.a = 1;
foo:bar(foo);

print("Magic: " .. Foo.magic);
