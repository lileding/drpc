use std::net::{ TcpListener, ToSocketAddrs, TcpStream };
use std::io::{ stderr, Read, Write, Result, ErrorKind };
use std::os::unix::io::{ RawFd, AsRawFd };
use std::rc::Rc;
use std::cell::RefCell;

mod event;

include!("acceptor.rs");
include!("channel.rs");

pub struct Server {
    eq: event::Queue,
    afd: RawFd,
}

impl Server {
    pub fn bind<A: ToSocketAddrs>(addr: A) -> Result<Server> {
        let accp = try!(Acceptor::bind(addr));
        let afd = accp.as_raw_fd();
        try!(accp.set_nonblocking(true));
        let eq = try!(event::Queue::new());
        try!(eq.watch(accp));
        let s = Server {
            eq: eq,
            afd: afd,
        };
        Ok(s)
    }

    pub fn run(&mut self) -> Result<()> {
        loop {
            try!(self.eq.wait(-1));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn simple() {
        let s = Server::bind("127.0.0.1:8001");
        assert!(s.is_ok());
        let rv = s.unwrap().run();
        assert!(rv.is_ok());
    }
}

