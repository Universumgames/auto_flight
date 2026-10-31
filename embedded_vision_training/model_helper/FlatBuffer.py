import struct

class FlatBuffer:
    """Just enough of a flatbuffer reader to walk tables, vectors and strings."""

    def __init__(self, data: bytes):
        self.data = data

    def u16(self, pos: int) -> int:
        return struct.unpack_from("<H", self.data, pos)[0]

    def u32(self, pos: int) -> int:
        return struct.unpack_from("<I", self.data, pos)[0]

    def i32(self, pos: int) -> int:
        return struct.unpack_from("<i", self.data, pos)[0]

    def root(self) -> int:
        return self.u32(0)

    def field(self, table: int, index: int) -> int | None:
        """Absolute position of field `index` of `table`, or None if it's absent (default value)."""
        vtable = table - self.i32(table)
        entry = 4 + 2 * index
        if entry >= self.u16(vtable):
            return None
        offset = self.u16(vtable + entry)
        return table + offset if offset else None

    def deref(self, pos: int) -> int:
        return pos + self.u32(pos)

    def tables(self, table: int, index: int) -> list[int]:
        pos = self.field(table, index)
        if pos is None:
            return []
        vector = self.deref(pos)
        return [self.deref(vector + 4 + 4 * i) for i in range(self.u32(vector))]

    def string(self, table: int, index: int) -> str | None:
        pos = self.field(table, index)
        if pos is None:
            return None
        string = self.deref(pos)
        return self.data[string + 4:string + 4 + self.u32(string)].decode()
