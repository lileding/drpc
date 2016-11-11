type Acceptor = TcpListener;

impl event::Target for Acceptor {
    fn mask() -> event::Mask {
        event::ReadEvent
    }
    fn handle(&mut self, queue: &mut event::Queue, mask: event::Mask) -> Result<()> {
        writeln!(stderr(), "on accept").unwrap_or(());
        loop {
            match self.accept() {
                Ok((s, addr)) => {
                    try!(s.set_nonblocking(true));
                    writeln!(stderr(), "got {}@{} with fd={}", s.local_addr().unwrap(), addr, s.as_raw_fd()).unwrap_or(());
                    try!(queue.watch(
                        Rc::new(RefCell::new(Box::new(Channel::new(s)))),
                        event::EventKind::ReadEdge
                    ));
                },
                Err(e) => {
                    if e.kind() == ErrorKind::WouldBlock {
                        return Ok(());
                    } else {
                        writeln!(stderr(), "error is {}", e).unwrap_or(());
                        return Err(e);
                    }
                }
            }
        }
    }
}

