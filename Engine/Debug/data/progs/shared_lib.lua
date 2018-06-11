-- shared_lib.lua

-- =====================================================
--
-- LIBRARIES
--
-- =====================================================

-- for function pointers as strings TODO: parameters? pass as fn, ... and call with fn(...)?
function function_call(fn)
	_G[fn]()
end

function function_get(fn)
	return _G[fn]
end

-- number of elements in a table
function tablelength(T)
	local count = 0
	for _ in pairs(T) do
		count = count + 1
	end
	return count
end

-- Print contents of "tbl", with indentation.
-- "indent" sets the initial level of indentation.
-- From https://gist.github.com/ripter/4270799
function tprint (tbl, indent)
	if not indent then
		indent = 0
	end
	for k, v in pairs(tbl) do
		formatting = string.rep("  ", indent) .. k .. ": "
		if type(v) == "table" then
			PrintC(formatting .. "\n")
			tprint(v, indent+1)
		elseif type(v) == 'boolean' then
			PrintC(formatting .. tostring(v) .. "\n")
		else
			PrintC(formatting .. v .. "\n")
		end
	end
end

-- empty/zero or nil checks
function isemptystring(s)
  return s == nil or s == ''
end

function isemptynumber(n)
  return n == nil or n == 0
end

-- splits a string
function split_string(str, delimiter)
	local ret = {}
	for word in string.gmatch(str, "([^" .. delimiter .. "]+)") do
		ret[#ret + 1] = word
	end
	return ret
end

--[[
	Author: Julio Manuel Fernandez-Diaz
	Date:   January 12, 2007
	(For Lua 5.1)

	Modified slightly by RiciLake to avoid the unnecessary table traversal in tablecount()

	Formats tables with cycles recursively to any depth.
	The output is returned as a string.
	References to other tables are shown as values.
	Self references are indicated.

	The string returned is "Lua code", which can be procesed
	(in the case in which indent is composed by spaces or "--").
	Userdata and function keys and values are shown as strings,
	which logically are exactly not equivalent to the original code.

	This routine can serve for pretty formating tables with
	proper indentations, apart from printing them:

		print(table.show(t, "t"))   -- a typical use

	Heavily based on "Saving tables with cycles", PIL2, p. 113.

	Arguments:
		t is the table.
		name is the name of the table (optional)
		indent is a first indentation (optional).

	Modified by Eluan Costa Miranda:
		Support table.serialize()
	
	This function doesn't work in this case, but it's a stretch:
	x = {1, 2, 3}
	x[x]=x
	print(table.show(x))

	--[[output:
	__unnamed__ = {
	   [1] = 1;
	   [2] = 2;
	   [3] = 3;
	   ["table: 0x695f08"] = {}; -- __unnamed__ (self reference)
	};
	__unnamed__["table: 0x695f08"] = __unnamed__;
	--]]
--]]
function table.show(t, name, indent)
   local cart     -- a container
   local autoref  -- for self references

   --[[ counts the number of elements in a table
   local function tablecount(t)
      local n = 0
      for _, _ in pairs(t) do n = n+1 end
      return n
   end
   ]]
   -- (RiciLake) returns true if the table is empty
   local function isemptytable(t) return next(t) == nil end

   local function basicSerialize (o)
      local so = tostring(o)
      if type(o) == "function" then
         local info = debug.getinfo(o, "S")
         -- info.name is nil because o is not a calling level
         if info.what == "C" then
            return string.format("%q", so .. ", C function")
         else 
            -- the information is defined through lines
            return string.format("%q", so .. ", defined in (" ..
                info.linedefined .. "-" .. info.lastlinedefined ..
                ")" .. info.source)
         end
      elseif type(o) == "number" or type(o) == "boolean" then
         return so
      else
         return string.format("%q", so)
      end
   end

   local function addtocart (value, name, indent, saved, field)
      indent = indent or ""
      saved = saved or {}
      field = field or name

      cart = cart .. indent .. field

      if type(value) ~= "table" then
         cart = cart .. " = " .. basicSerialize(value) .. ";\n"
      else
         if saved[value] then
            cart = cart .. " = {}; -- " .. saved[value] 
                        .. " (self reference)\n"
            autoref = autoref ..  name .. " = " .. saved[value] .. ";\n"
         else
            saved[value] = name
            --if tablecount(value) == 0 then
			if value.serialize then
				cart = cart .. " = " .. value:serialize() .. ";\n"
            elseif isemptytable(value) then
               cart = cart .. " = {};\n"
            else
               cart = cart .. " = {\n"
               for k, v in pairs(value) do
                  k = basicSerialize(k)
                  local fname = string.format("%s[%s]", name, k)
                  field = string.format("[%s]", k)
                  -- three spaces between levels
                  addtocart(v, fname, indent .. "   ", saved, field)
               end
               cart = cart .. indent .. "};\n"
            end
         end
      end
   end

   name = name or "__unnamed__"
   if type(t) ~= "table" then
      return name .. " = " .. basicSerialize(t)
   end
   cart, autoref = "", ""
   addtocart(t, name, indent)
   return cart .. autoref
end

-- 2d vector
Vector2 = {}
Vector2.__index = Vector2

function Vector2.__add(a, b)
  if type(a) == "number" then
    return Vector2.new(b.x + a, b.y + a)
  elseif type(b) == "number" then
    return Vector2.new(a.x + b, a.y + b)
  else
    return Vector2.new(a.x + b.x, a.y + b.y)
  end
end

function Vector2.__sub(a, b)
  if type(a) == "number" then
    return Vector2.new(a - b.x, a - b.y)
  elseif type(b) == "number" then
    return Vector2.new(a.x - b, a.y - b)
  else
    return Vector2.new(a.x - b.x, a.y - b.y)
  end
end

function Vector2.__mul(a, b)
  if type(a) == "number" then
    return Vector2.new(b.x * a, b.y * a)
  elseif type(b) == "number" then
    return Vector2.new(a.x * b, a.y * b)
  else
    return Vector2.new(a.x * b.x, a.y * b.y) -- simd, sort of
  end
end

function Vector2.__div(a, b)
  if type(a) == "number" then
    return Vector2.new(a / b.x, a / b.y)
  elseif type(b) == "number" then
    return Vector2.new(a.x / b, a.y / b)
  else
    return Vector2.new(a.x / b.x, a.y / b.y) -- simd, sort of
  end
end

function Vector2.__eq(a, b)
  return a.x == b.x and a.y == b.y
end

function Vector2.__lt(a, b)
  return a.x < b.x or (a.x == b.x and a.y < b.y)
end

function Vector2.__le(a, b)
  return a.x <= b.x and a.y <= b.y
end

function Vector2.__unm(a)
  return Vector2.new(-a.x, -a.y)
end

function Vector2.__tostring(a)
  return "(" .. a.x .. " " .. a.y .. ")"
end

function Vector2:serialize()
  return "Vector2(" .. self.x .. ", " .. self.y .. ")"
end

function Vector2.new(x, y)
  return setmetatable({ x = x or 0, y = y or 0 }, Vector2)
end

function Vector2.distance(a, b)
  return (b - a):len()
end

-- TODO: is this slow?
function Vector2.fromstring(str) -- format "x y" or "(x y)"
	local x, y
	local t = {}
	-- remove parenthesis, if present
	str = string.gsub(str, "%((.-)%)", "%1")
	for n in str:gmatch("%S+") do
		table.insert(t, tonumber(n))
	end
	assert(tablelength(t) == 2)
	x, y = table.unpack(t)

	return Vector2(x, y)
end

function Vector2:clone()
  return Vector2.new(self.x, self.y)
end

function Vector2:unpack()
  return self.x, self.y
end

function Vector2:len()
  return math.sqrt(self:dot(self))
end

function Vector2:lenSq()
  return self:dot(self)
end

function Vector2:normalize()
  local invlen
  local len = self:len()
  if len > 0 then
	invlen = 1.0 / len
  else
    return self -- fail silently for null vectors
  end
  self.x = self.x * invlen
  self.y = self.y * invlen
  return self
end

function Vector2:normalized()
  local invlen
  local len = self:len()
  if len > 0 then
	invlen = 1.0 / len
  else
    return self:clone() -- fail silently for null vectors
  end
  return self * invlen
end

function Vector2:rotate(phi)
  local c = math.cos(phi)
  local s = math.sin(phi)
  self.x = c * self.x - s * self.y
  self.y = s * self.x + c * self.y
  return self
end

function Vector2:rotated(phi)
  return self:clone():rotate(phi)
end

function Vector2:perpendicular()
  return Vector2.new(-self.y, self.x)
end

function Vector2:projectOn(other)
  return (self * other) * other / other:lenSq()
end

function Vector2:dot(other)
  return self.x * other.x + self.y * other.y
end

function Vector2:cross(other)
  return self.x * other.y - self.y * other.x -- magnitude of a 3d cross product if Z was zero
end

function Vector2:clear()
  self.x = 0
  self.y = 0
  return self
end

function Vector2:iszero()
  return self.x == 0 and self.y == 0
end

function Vector2:set(ix, iy)
  self.x = ix
  self.y = iy
  return self
end

function Vector2:CheckIfClose(other, maxclosedist)
	if (other - self):lenSq() <= maxclosedist * maxclosedist then
		return true
	else
		return false
	end
end

Vector2_metatable = { __call = function(_, ...) return Vector2.new(...) end }
setmetatable(Vector2, Vector2_metatable)

-- 3d vector
Vector3 = {}
Vector3.__index = Vector3

function Vector3.__add(a, b)
  if type(a) == "number" then
    return Vector3.new(b.x + a, b.y + a, b.z + a)
  elseif type(b) == "number" then
    return Vector3.new(a.x + b, a.y + b, a.z + b)
  else
    return Vector3.new(a.x + b.x, a.y + b.y, a.z + b.z)
  end
end

function Vector3.__sub(a, b)
  if type(a) == "number" then
    return Vector3.new(a - b.x, a - b.y, a - b.z)
  elseif type(b) == "number" then
    return Vector3.new(a.x - b, a.y - b, a.z - b)
  else
    return Vector3.new(a.x - b.x, a.y - b.y, a.z - b.z)
  end
end

function Vector3.__mul(a, b)
  if type(a) == "number" then
    return Vector3.new(b.x * a, b.y * a, b.z * a)
  elseif type(b) == "number" then
    return Vector3.new(a.x * b, a.y * b, a.z * b)
  else
    return Vector3.new(a.x * b.x, a.y * b.y, a.z * b.z) -- simd, sort of
  end
end

function Vector3.__div(a, b)
  if type(a) == "number" then
    return Vector3.new(a / b.x, a / b.y, a / b.z)
  elseif type(b) == "number" then
    return Vector3.new(a.x / b, a.y / b, a.z / b)
  else
    return Vector3.new(a.x / b.x, a.y / b.y, a.z / b.z) -- simd, sort of
  end
end

function Vector3.__eq(a, b)
  return a.x == b.x and a.y == b.y and a.z == b.z
end

function Vector3.__lt(a, b)
  return a.x < b.x or (a.x == b.x and a.y < b.y) or (a.x == b.x and a.y == b.y and a.z < b.z)
end

function Vector3.__le(a, b)
  return a.x <= b.x and a.y <= b.y and a.z <= b.z
end

function Vector3.__unm(a)
  return Vector3.new(-a.x, -a.y, -a.z)
end

function Vector3.__tostring(a)
  return "(" .. a.x .. " " .. a.y .. " " .. a.z .. ")"
end

function Vector3:serialize()
  return "Vector3(" .. self.x .. ", " .. self.y .. ", " .. self.z .. ")"
end

function Vector3.new(x, y, z)
  return setmetatable({ x = x or 0, y = y or 0, z = z or 0 }, Vector3)
end

function Vector3.distance(a, b)
  return (b - a):len()
end

-- TODO: is this slow?
function Vector3.fromstring(str) -- format "x y z" or "(x y z)"
	local x, y, z
	local t = {}
	-- remove parenthesis, if present
	str = string.gsub(str, "%((.-)%)", "%1")
	for n in str:gmatch("%S+") do
		table.insert(t, tonumber(n))
	end
	assert(tablelength(t) == 3)
	x, y, z = table.unpack(t)

	return Vector3(x, y, z)
end

function Vector3:clone()
  return Vector3.new(self.x, self.y, self.z)
end

function Vector3:unpack()
  return self.x, self.y, self.z
end

function Vector3:len()
  return math.sqrt(self:dot(self))
end

function Vector3:lenSq()
  return self:dot(self)
end

function Vector3:normalize()
  local invlen
  local len = self:len()
  if len > 0 then
	invlen = 1.0 / len
  else
    return self -- fail silently for null vectors
  end
  self.x = self.x * invlen
  self.y = self.y * invlen
  self.z = self.z * invlen
  return self
end

function Vector3:normalized()
  local invlen
  local len = self:len()
  if len > 0 then
	invlen = 1.0 / len
  else
    return self:clone() -- fail silently for null vectors
  end
  return self * invlen
end

-- TODO
--function Vector3:rotate(phi)
--  local c = math.cos(phi)
--  local s = math.sin(phi)
--  self.x = c * self.x - s * self.y
--  self.y = s * self.x + c * self.y
--  return self
--end

--function Vector3:rotated(phi)
--  return self:clone():rotate(phi)
--end

--function Vector3:perpendicular()
--  return Vector3.new(-self.y, self.x)
--end

--function Vector3:projectOn(other)
--  return (self * other) * other / other:lenSq()
--end

function Vector3:dot(other)
  return self.x * other.x + self.y * other.y + self.z * other.z
end

function Vector3:cross(other)
  return Vector3(self.y * other.z - self.z * other.y, self.z * other.x - self.x * other.z, self.x * other.y - self.y * other.x)
end

function Vector3:clear()
  self.x = 0
  self.y = 0
  self.z = 0
  return self
end

function Vector3:iszero()
  return self.x == 0 and self.y == 0 and self.z == 0
end

function Vector3:set(ix, iy, iz)
  self.x = ix
  self.y = iy
  self.z = iz
  return self
end

function Vector3:setAngle(which, angle)
  if which == ANGLES_PITCH then
    self.x = angle
    return self.x
  elseif which == ANGLES_YAW then
    self.y = angle
    return self.y
  elseif which == ANGLES_ROLL then
    self.z = angle
    return self.z
  else
    error("invalid angle")
  end
end

function Vector3:getAngle(which)
  if which == ANGLES_PITCH then
    return self.x
  elseif which == ANGLES_YAW then
    return self.y
  elseif which == ANGLES_ROLL then
    return self.z
  else
    error("invalid angle")
  end
end

function Vector3:CheckIfClose(other, maxclosedist)
	if (other - self):lenSq() <= maxclosedist * maxclosedist then
		return true
	else
		return false
	end
end

Vector3_metatable = { __call = function(_, ...) return Vector3.new(...) end }
setmetatable(Vector3, Vector3_metatable)

-- 4d vector
Vector4 = {}
Vector4.__index = Vector4

function Vector4.__add(a, b)
  if type(a) == "number" then
    return Vector4.new(b.x + a, b.y + a, b.z + a, b.w + a)
  elseif type(b) == "number" then
    return Vector4.new(a.x + b, a.y + b, a.z + b, a.w + b)
  else
    return Vector4.new(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w)
  end
end

function Vector4.__sub(a, b)
  if type(a) == "number" then
    return Vector4.new(a - b.x, a - b.y, a - b.z, a - b.w)
  elseif type(b) == "number" then
    return Vector4.new(a.x - b, a.y - b, a.z - b, a.w - b)
  else
    return Vector4.new(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w)
  end
end

function Vector4.__mul(a, b)
  if type(a) == "number" then
    return Vector4.new(b.x * a, b.y * a, b.z * a, b.w * a)
  elseif type(b) == "number" then
    return Vector4.new(a.x * b, a.y * b, a.z * b, a.w * b)
  else
    return Vector4.new(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w) -- simd, sort of
  end
end

function Vector4.__div(a, b)
  if type(a) == "number" then
    return Vector4.new(a / b.x, a / b.y, a / b.z, a / b.w)
  elseif type(b) == "number" then
    return Vector4.new(a.x / b, a.y / b, a.z / b, a.w / b)
  else
    return Vector4.new(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w) -- simd, sort of
  end
end

function Vector4.__eq(a, b)
  return a.x == b.x and a.y == b.y and a.z == b.z and a.w == b.w
end

function Vector4.__lt(a, b)
  return a.x < b.x or (a.x == b.x and a.y < b.y) or (a.x == b.x and a.y == b.y and a.z < b.z) or (a.x == b.x and a.y == b.y and a.z == b.z and a.w < b.w)
end

function Vector4.__le(a, b)
  return a.x <= b.x and a.y <= b.y and a.z <= b.z and a.w <= b.w
end

function Vector4.__unm(a)
  return Vector4.new(-a.x, -a.y, -a.z, -a.w)
end

function Vector4.__tostring(a)
  return "(" .. a.x .. " " .. a.y .. " " .. a.z .. " " .. a.w .. ")"
end

function Vector4:serialize()
  return "Vector4(" .. self.x .. ", " .. self.y .. ", " .. self.z .. ", " .. self.w .. ")"
end

function Vector4.new(x, y, z, w)
  return setmetatable({ x = x or 0, y = y or 0, z = z or 0, w = w or 0 }, Vector4)
end

function Vector4.distance(a, b)
  return (b - a):len()
end

-- TODO: is this slow?
function Vector4.fromstring(str) -- format "x y z w" or "(x y z w)"
	local x, y, z, w
	local t = {}
	-- remove parenthesis, if present
	str = string.gsub(str, "%((.-)%)", "%1")
	for n in str:gmatch("%S+") do
		table.insert(t, tonumber(n))
	end
	assert(tablelength(t) == 4)
	x, y, z, w = table.unpack(t)

	return Vector4(x, y, z, w)
end

function Vector4:clone()
  return Vector4.new(self.x, self.y, self.z, self.w)
end

function Vector4:unpack()
  return self.x, self.y, self.z, self.w
end

function Vector4:len()
  return math.sqrt(self:dot(self))
end

function Vector4:lenSq()
  return self:dot(self)
end

function Vector4:normalize()
  local invlen
  local len = self:len()
  if len > 0 then
	invlen = 1.0 / len
  else
    return self -- fail silently for null vectors
  end
  self.x = self.x * invlen
  self.y = self.y * invlen
  self.z = self.z * invlen
  self.w = self.w * invlen
  return self
end

function Vector4:normalized()
  local invlen
  local len = self:len()
  if len > 0 then
	invlen = 1.0 / len
  else
    return self:clone() -- fail silently for null vectors
  end
  return self * invlen
end

function Vector4:dot(other)
  return self.x * other.x + self.y * other.y + self.z * other.z + self.w * other.w
end

function Vector4:clear()
  self.x = 0
  self.y = 0
  self.z = 0
  self.w = 0
  return self
end

function Vector4:iszero()
  return self.x == 0 and self.y == 0 and self.z == 0 and self.w == 0
end

function Vector4:set(ix, iy, iz, iw)
  self.x = ix
  self.y = iy
  self.z = iz
  self.w = iw
  return self
end

Vector4_metatable = { __call = function(_, ...) return Vector4.new(...) end }
setmetatable(Vector4, Vector4_metatable)

-- 4x4 matrix
Matrix4x4 = {}
Matrix4x4.__index = Matrix4x4

function Matrix4x4.__add(a, b)
  error("unsupported matrix operation")
end

function Matrix4x4.__sub(a, b)
	error("unsupported matrix operation")
end

function Matrix4x4.__mul(a, b)
	local tmp = {}

	-- column 0
	tmp[0] = a[0] * b[0] + a[4] * b[1] + a[8 ] * b[2] + a[12] * b[3]
	tmp[1] = a[1] * b[0] + a[5] * b[1] + a[9 ] * b[2] + a[13] * b[3]
	tmp[2] = a[2] * b[0] + a[6] * b[1] + a[10] * b[2] + a[14] * b[3]
	tmp[3] = a[3] * b[0] + a[7] * b[1] + a[11] * b[2] + a[15] * b[3]

	-- column 1
	tmp[4] = a[0] * b[4] + a[4] * b[5] + a[8 ] * b[6] + a[12] * b[7]
	tmp[5] = a[1] * b[4] + a[5] * b[5] + a[9 ] * b[6] + a[13] * b[7]
	tmp[6] = a[2] * b[4] + a[6] * b[5] + a[10] * b[6] + a[14] * b[7]
	tmp[7] = a[3] * b[4] + a[7] * b[5] + a[11] * b[6] + a[15] * b[7]

	-- column 2
	tmp[8 ] = a[0] * b[8] + a[4] * b[9] + a[8 ] * b[10] + a[12] * b[11]
	tmp[9 ] = a[1] * b[8] + a[5] * b[9] + a[9 ] * b[10] + a[13] * b[11]
	tmp[10] = a[2] * b[8] + a[6] * b[9] + a[10] * b[10] + a[14] * b[11]
	tmp[11] = a[3] * b[8] + a[7] * b[9] + a[11] * b[10] + a[15] * b[11]

	-- column 3
	tmp[12] = a[0] * b[12] + a[4] * b[13] + a[8 ] * b[14] + a[12] * b[15]
	tmp[13] = a[1] * b[12] + a[5] * b[13] + a[9 ] * b[14] + a[13] * b[15]
	tmp[14] = a[2] * b[12] + a[6] * b[13] + a[10] * b[14] + a[14] * b[15]
	tmp[15] = a[3] * b[12] + a[7] * b[13] + a[11] * b[14] + a[15] * b[15]
	
	return Matrix4x4(tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5], tmp[6], tmp[7], tmp[8], tmp[9], tmp[10], tmp[11], tmp[12], tmp[13], tmp[14], tmp[15])
end

function Matrix4x4.__div(a, b)
	error("unsupported matrix operation")
end

function Matrix4x4.__eq(a, b)
  return a[0] == b[0] and a[1] == b[1] and a[2] == b[2] and a[3] == b[3] and a[4] == b[4] and a[5] == b[5] and a[6] == b[6] and a[7] == b[7] and a[8] == b[8] and a[9] == b[9] and a[10] == b[10] and a[11] == b[11] and a[12] == b[12] and a[13] == b[13] and a[14] == b[14] and a[15] == b[15]
end

function Matrix4x4.__lt(a, b)
  error("unsupported matrix operation")
end

function Matrix4x4.__le(a, b)
  error("unsupported matrix operation")
end

function Matrix4x4.__unm(a)
  error("unsupported matrix operation")
end

function Matrix4x4.__tostring(a)
  return "(" .. a[0] .. " " .. a[1] .. " " .. a[2] .. " " .. a[3] .. " " .. a[4] .. " " .. a[5] .. " " .. a[6] .. " " .. a[7] .. " " .. a[8] .. " " .. a[9] .. " " .. a[10] .. " " .. a[11] .. " " .. a[12] .. " " .. a[13] .. " " .. a[14] .. " " .. a[15] .. ")"
end

function Matrix4x4:serialize()
  return "Matrix4x4(" .. self[0] .. ", " .. self[1] .. ", " .. self[2] .. ", " .. self[3] .. ", " .. self[4] .. ", " .. self[5] .. ", " .. self[6] .. ", " .. self[7] .. ", " .. self[8] .. ", " .. self[9] .. ", " .. self[10] .. ", " .. self[11] .. ", " .. self[12] .. ", " .. self[13] .. ", " .. self[14] .. ", " .. self[15] .. ")"
end

function Matrix4x4.new(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15)
  return setmetatable({[0] = m0 or 1, [1] = m1 or 0, [2] = m2 or 0, [3] = m3 or 0, [4] = m4 or 0, [5] = m5 or 1, [6] = m6 or 0, [7] = m7 or 0, [8] = m8 or 0, [9] = m9 or 0, [10] = m10 or 1, [11] = m11 or 0, [12] = m12 or 0, [13] = m13 or 0, [14] = m14 or 0, [15] = m15 or 1}, Matrix4x4)
end

function Matrix4x4:clone()
  return Matrix4x4.new(self[0], self[1], self[2], self[3], self[4], self[5], self[6], self[7], self[8], self[9], self[10], self[11], self[12], self[13], self[14], self[15])
end

function Matrix4x4:unpack()
  return self[0], self[1], self[2], self[3], self[4], self[5], self[6], self[7], self[8], self[9], self[10], self[11], self[12], self[13], self[14], self[15]
end

function Matrix4x4:applyToVector3(v)
	return Vector3(self[0] * v.x + self[4] * v.y + self[8] * v.z + self[12] * 1, self[1] * v.x + self[5] * v.y + self[9] * v.z + self[13] * 1, self[2] * v.x + self[6] * v.y + self[10] * v.z + self[14] * 1)
end

function Matrix4x4:inverse()
	local inv = {}
	local det

    inv[0] = self[5]  * self[10] * self[15] - self[5]  * self[11] * self[14] - self[9]  * self[6]  * self[15] + self[9]  * self[7]  * self[14] + self[13] * self[6]  * self[11] - self[13] * self[7]  * self[10]
    inv[4] = -self[4]  * self[10] * self[15] + self[4]  * self[11] * self[14] + self[8]  * self[6]  * self[15] - self[8]  * self[7]  * self[14] - self[12] * self[6]  * self[11] + self[12] * self[7]  * self[10]
    inv[8] = self[4]  * self[9] * self[15] - self[4]  * self[11] * self[13] - self[8]  * self[5] * self[15] + self[8]  * self[7] * self[13] + self[12] * self[5] * self[11] - self[12] * self[7] * self[9]
    inv[12] = -self[4]  * self[9] * self[14] + self[4]  * self[10] * self[13] +self[8]  * self[5] * self[14] - self[8]  * self[6] * self[13] - self[12] * self[5] * self[10] + self[12] * self[6] * self[9]
    inv[1] = -self[1]  * self[10] * self[15] + self[1]  * self[11] * self[14] + self[9]  * self[2] * self[15] - self[9]  * self[3] * self[14] - self[13] * self[2] * self[11] + self[13] * self[3] * self[10]
    inv[5] = self[0]  * self[10] * self[15] - self[0]  * self[11] * self[14] - self[8]  * self[2] * self[15] + self[8]  * self[3] * self[14] + self[12] * self[2] * self[11] - self[12] * self[3] * self[10]
    inv[9] = -self[0]  * self[9] * self[15] + self[0]  * self[11] * self[13] + self[8]  * self[1] * self[15] - self[8]  * self[3] * self[13] - self[12] * self[1] * self[11] + self[12] * self[3] * self[9]
    inv[13] = self[0]  * self[9] * self[14] - self[0]  * self[10] * self[13] - self[8]  * self[1] * self[14] + self[8]  * self[2] * self[13] + self[12] * self[1] * self[10] - self[12] * self[2] * self[9]
    inv[2] = self[1]  * self[6] * self[15] - self[1]  * self[7] * self[14] - self[5]  * self[2] * self[15] + self[5]  * self[3] * self[14] + self[13] * self[2] * self[7] - self[13] * self[3] * self[6]
    inv[6] = -self[0]  * self[6] * self[15] + self[0]  * self[7] * self[14] + self[4]  * self[2] * self[15] - self[4]  * self[3] * self[14] - self[12] * self[2] * self[7] + self[12] * self[3] * self[6]
    inv[10] = self[0]  * self[5] * self[15] - self[0]  * self[7] * self[13] - self[4]  * self[1] * self[15] + self[4]  * self[3] * self[13] + self[12] * self[1] * self[7] - self[12] * self[3] * self[5]
    inv[14] = -self[0]  * self[5] * self[14] + self[0]  * self[6] * self[13] + self[4]  * self[1] * self[14] - self[4]  * self[2] * self[13] - self[12] * self[1] * self[6] + self[12] * self[2] * self[5]
    inv[3] = -self[1] * self[6] * self[11] + self[1] * self[7] * self[10] + self[5] * self[2] * self[11] - self[5] * self[3] * self[10] - self[9] * self[2] * self[7] + self[9] * self[3] * self[6]
    inv[7] = self[0] * self[6] * self[11] - self[0] * self[7] * self[10] - self[4] * self[2] * self[11] + self[4] * self[3] * self[10] + self[8] * self[2] * self[7] - self[8] * self[3] * self[6]
    inv[11] = -self[0] * self[5] * self[11] + self[0] * self[7] * self[9] + self[4] * self[1] * self[11] - self[4] * self[3] * self[9] - self[8] * self[1] * self[7] + self[8] * self[3] * self[5]
    inv[15] = self[0] * self[5] * self[10] - self[0] * self[6] * self[9] - self[4] * self[1] * self[10] + self[4] * self[2] * self[9] + self[8] * self[1] * self[6] - self[8] * self[2] * self[5]

    det = self[0] * inv[0] + self[1] * inv[4] + self[2] * inv[8] + self[3] * inv[12]

    if det == 0 then
		error("Matrix4x4:inverse: determinant == zero, matrix is not invertible: " .. tostring(self) .. "\n")
	end

    det = 1.0 / det

    return Matrix4x4(inv[0] * det, inv[1] * det, inv[2] * det, inv[3] * det, inv[4] * det, inv[5] * det, inv[6] * det, inv[7] * det, inv[8] * det, inv[9] * det, inv[10] * det, inv[11] * det, inv[12] * det, inv[13] * det, inv[14] * det, inv[15] * det)
end

function Matrix4x4:transpose()
	return Matrix4x4(m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13], m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15])
end

-- projection
function Matrix4x4.PerspectiveFrustum(left, right, bottom, top, near, far)
	return Matrix4x4(2.0 * near / (right - left), 0, 0, 0, 0, 2.0 * near / (top - bottom), 0, 0, (right + left) / (right - left), (top + bottom) / (top - bottom), -((far + near) / (far - near)), -1, 0, 0, -(2.0 * far * near) / (far - near), 0)
end

-- projection
function Matrix4x4.Ortho(left, right, bottom, top, near, far)
	return Matrix4x4(2.0 / (right - left), 0, 0, 0, 0, 2.0 / (top - bottom), 0, 0, 0, 0, -2.0 / (far - near), 0, -((right + left) / (right - left)), -((top + bottom) / (top - bottom)), -((far + near) / (far - near)), 1)
end

-- view
function Matrix4x4.LookAt(eyex, eyey, eyez, centerx, centery, centerz, upx, upy, upz)
	local forward, side, up, tmp

	forward = Vector3(centerx - eyex, centery - eyey, centerz - eyez)
	up = Vector3(upx, upy, upz)

	forward:normalize()

	-- side = forward x up
	side = forward:cross(up)
	side:normalize()

	-- recompute up as: up = side x forward
	up = side:cross(forward)

	-- since we need the inverse transformation for the view matrix and this matrix is orthonormal, the transpose will be the inverse
	tmp = Matrix4x4(side.x, up.x, -forward.x, 0, side.y, up.y, -forward.y, 0, side.z, up.z, -forward.z, 0, 0, 0, 0, 1)

	return tmp:translate(-eyex, -eyey, -eyez)
end

function Matrix4x4.Identity()
	return Matrix4x4()
end

function Matrix4x4:Translate(x, y, z)
	local tmp = Matrix4x4()

	tmp[12]= x
	tmp[13]= y
	tmp[14]= z

	return self * tmp
end

function Matrix4x4:Scale(x, y, z)
	local tmp = Matrix4x4()

	tmp[0] = x
	tmp[5] = y
	tmp[10]= z

	return self * tmp
end

function Matrix4x4:RotateX(angle)
	local tmp = Matrix4x4()
	local radians = math.rad(angle)

	tmp[5] = math.cos(radians)
	tmp[6] = math.sin(radians)
	tmp[9] = -tmp[6]
	tmp[10] = tmp[5]

	return self * tmp
end

function Matrix4x4:RotateY(angle)
	local tmp = Matrix4x4()
	local radians = math.rad(angle)

	tmp[0] = math.cos(radians)
	tmp[2] = -math.sin(radians)
	tmp[8] = -tmp[2]
	tmp[10] = tmp[0]

	return self * tmp
end

function Matrix4x4:RotateZ(angle)
	local tmp = Matrix4x4()
	local radians = math.rad(angle)

	tmp[0] = math.cos(radians)
	tmp[1] = math.sin(radians)
	tmp[4] = -tmp[1]
	tmp[5] = tmp[0]

	return self * tmp
end

function Matrix4x4:RotateFromVectors(forward, right, up)
	local tmp = Matrix4x4()

	tmp[8] = -forward.x;
	tmp[9] = -forward.y;
	tmp[10] = -forward.z;

	tmp[0] = right.x;
	tmp[1] = right.y;
	tmp[2] = right.z;

	tmp[4] = up.x;
	tmp[5] = up.y;
	tmp[6] = up.z;

	return self * tmp
end

Matrix4x4_metatable = { __call = function(_, ...) return Matrix4x4.new(...) end }
setmetatable(Matrix4x4, Matrix4x4_metatable)
