extern crate libc;

use std::io::{ Error, Result };
use std::os::unix::io::AsRawFd;
use std::rc::Rc;
use std::cell::RefCell;
use std::io::{ stderr, Write };

pub trait Handler : AsRawFd {
    fn handle(&mut self, queue: &mut Queue) -> Result<()>;
}

pub struct Queue {
    epfd: libc::c_int,
    waiting: Vec<Rc<RefCell<Box<Handler>>>>,
}

pub enum EventKind {
    ReadLevel = libc::EPOLLIN as isize,
    ReadEdge = (libc::EPOLLIN | libc::EPOLLET) as isize,
}

impl Queue {
    pub fn new() -> Result<Queue> {
        let epfd = unsafe { libc::epoll_create(1) };
        if epfd == -1 {
            Err(Error::last_os_error())
        } else {
            Ok(Queue {
                epfd: epfd,
                waiting: vec![],
            })
        }
    }

    pub fn wait(&mut self, timeout: i64) -> Result<Vec<Rc<RefCell<Box<Handler>>>>> {
        let mut active = [libc::epoll_event { events: 0, u64: 0 }; 32];
        let rv = unsafe { libc::epoll_wait(
            self.epfd,
            active.as_mut_ptr(),
            active.len() as libc::c_int,
            timeout as libc::c_int
        )};
        if rv == -1 {
            Err(Error::last_os_error())
        } else {
            let mut v: Vec<Rc<RefCell<Box<Handler>>>> = vec![];
            for e in active.iter().take(rv as usize) {
                v.push(self.waiting[e.u64 as usize].clone());
            }
            Ok(v)
        }
    }

    pub fn watch(&mut self, h: Rc<RefCell<Box<Handler>>>, e: EventKind) -> Result<()> {
        let fd = h.as_ref().borrow().as_raw_fd();
        self.waiting.push(h);
        let idx = self.waiting.len() - 1;
        writeln!(stderr(), "watch {} at {}", fd, idx).unwrap_or(());
        let mut ev = libc::epoll_event {
            events: e as libc::uint32_t,
            u64: idx as libc::uint64_t,
        };
        let rv = unsafe { libc::epoll_ctl(
            self.epfd,
            libc::EPOLL_CTL_ADD,
            fd as libc::c_int,
            &mut ev as *mut libc::epoll_event)
        };
        if rv == -1 {
            Err(Error::last_os_error())
        } else {
            Ok(())
        }
    }
}

impl Drop for Queue {
    fn drop(&mut self) {
        if self.epfd != -1 {
            unsafe { libc::close(self.epfd); };
        }
    }
}

