
struct Channel {
    stream: TcpStream,
    operate: fn (&mut Channel) -> ChannelState,
    offset: usize,
    header: [u8; 4],
    body: Vec<u8>,
}

enum JobResult {
    Complete,
    DoneWith(usize),
    Err(std::io::Error),
}

enum ChannelState {
    Future,
    Continue,
    Err(std::io::Error),
}

impl Channel {
    fn new(s: TcpStream) -> Channel {
        writeln!(stderr(), "ctor, fd={}", s.as_raw_fd()).unwrap_or(());
        Channel {
            stream: s,
            operate: Self::recv_header,
            offset: 0,
            header: [0u8; 4],
            body: Vec::new(),
        }
    }

    fn recv_header(&mut self) -> ChannelState {
        match Self::recv(&mut self.stream, &mut self.header[self.offset..]) {
            JobResult::Complete => {
                let body_size: u32 =
                    (self.header[0] as u32) << 24 |
                    (self.header[1] as u32) << 16 |
                    (self.header[2] as u32) <<  8 |
                    (self.header[3] as u32) <<  0;
                //writeln!(stderr(), "body size = {}", body_size).unwrap_or(());
                //self.body = String::with_capacity(body_size as usize);
                self.body.resize(body_size as usize, 0u8);
                self.offset = 0;
                self.operate = Self::recv_body;
                ChannelState::Continue
            },
            JobResult::DoneWith(sz) => {
                self.offset += sz;
                ChannelState::Future
            },
            JobResult::Err(e) => ChannelState::Err(e),
        }
    }

    fn recv_body(&mut self) -> ChannelState {
        let mut buf = self.body.as_mut_slice();
        //writeln!(stderr(), "recv_body, offset={}, len={}", self.offset, buf.len()).unwrap_or(());
        match Self::recv(&mut self.stream, &mut buf[self.offset..]) {
            JobResult::Complete => {
                //writeln!(stderr(), "body = {}", unsafe { String::from_raw_parts(buf.as_mut_ptr(), buf.len(), buf.len()) }).unwrap_or(());
                self.offset = 0;
                self.operate = Self::recv_header;
                ChannelState::Continue
            },
            JobResult::DoneWith(sz) => {
                self.offset += sz;
                ChannelState::Future
            },
            JobResult::Err(e) => ChannelState::Err(e),
        }
    }

    fn recv(s: &mut TcpStream, buf: &mut [u8]) -> JobResult {
        let mut need = buf.len();
        let mut offset: usize = 0;
        while need > 0 {
            let (_, mut cur) = buf.split_at_mut(offset);
            match s.read(&mut cur) {
                Ok(sz) => {
                    if sz == 0 {
                        return JobResult::DoneWith(0);
                    }
                    need -= sz;
                    offset += sz;
                },
                Err(e) => {
                    if e.kind() == ErrorKind::WouldBlock {
                        return JobResult::DoneWith(offset);
                    } else {
                        return JobResult::Err(e);
                    }
                }
            }
        }
        JobResult::Complete
    }
}

impl event::Target for Channel {
    fn handle(&mut self, _: &mut event::Queue) -> Result<()> {
        loop {
            match (self.operate)(self) {
                ChannelState::Continue => { continue; },
                ChannelState::Future => { return Ok(()); },
                ChannelState::Err(e) => { return Err(e); },
            }
        }
    }
}

impl AsRawFd for Channel {
    fn as_raw_fd(&self) -> RawFd {
        writeln!(stderr(), "fd={}", self.stream.as_raw_fd()).unwrap_or(());
        self.stream.as_raw_fd()
    }
}

