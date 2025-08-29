const std = @import("std");

pub fn MessageQueue(T: type) type {
    return struct {
        mutex: std.Thread.Mutex,
        condition: std.Thread.Condition,
        buffer: []T,
        produced_index: usize = 0,
        consumed_index: usize = 0,

        pub fn init(buffer: []T) @This() {
            return .{
                .mutex = std.Thread.Mutex{},
                .condition = std.Thread.Condition{},
                .buffer = buffer,
            };
        }

        pub fn enqueue(self: *@This(), value: T) void {
            std.debug.assert(self.produced_index - self.consumed_index < self.buffer.len);
            // while (self.produced_index - self.consumed_index > self.buffer.len) {
            //     // wait consumer
            //     std.Thread.sleep(std.time.ns_per_ms * 66);
            // }
            {
                self.mutex.lock();
                defer self.mutex.unlock();
                self.buffer[self.produced_index % self.buffer.len] = value;
                self.produced_index += 1;
            }
            self.condition.signal();
        }

        pub fn dequeue(self: *@This()) T {
            self.mutex.lock();
            defer self.mutex.unlock();

            if (self.produced_index <= self.consumed_index) {
                self.condition.wait(&self.mutex);
            }

            defer self.consumed_index += 1;
            return self.buffer[self.consumed_index % self.buffer.len];
        }
    };
}
