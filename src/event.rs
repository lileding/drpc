extern crate libc;

use std::io::{ Error, Result };
use std::os::unix::io::AsRawFd;
use std::rc::Rc;
use std::cell::RefCell;
use std::io::{ stderr, Write };
use std::collections::BTreeMap;

pub trait Target : AsRawFd {
    fn handle(&mut self, queue: &mut Queue, mask: Mask) -> Result<()>;
    fn mask(): Mask;
}

pub type Mask = u32;

pub const ReadEvent: u32 = libc::EPOLLIN;
pub const WriteEvent: u32 = libc::EPOLLOUT;
pub const EdgeTrigger: u32 = libc::EPOLLET;

pub struct Queue {
    qfd: libc::c_int,
    pending: BTreeMap<RawFd, Box<Target>>,
}

impl Queue {
    pub fn new() -> Result<Queue> {
        let qfd = unsafe { libc::epoll_create(1) };
        if qfd == -1 {
            Err(Error::last_os_error())
        } else {
            Ok(Queue {
                qfd: qfd,
                waiting: vec![],
            })
        }
    }

    pub fn wait(&mut self, timeout: i64) -> Result<()> {
        let mut active = [libc::epoll_event { events: 0, u64: 0 }; 32];
        let rv = unsafe { libc::epoll_wait(
            self.qfd,
            active.as_mut_ptr(),
            active.len() as libc::c_int,
            timeout as libc::c_int
        )};
        if rv == -1 {
            Err(Error::last_os_error())
        } else {
            for e in active.iter().take(rv as usize) {
                let mut h = try!(self.pending.get_mut(e.u64 as RawFd));
                h.handle(self, e.events as Mask).unwrap_or_else(|e| {
                    writeln!(stderr(), "handle fail: {:?}", e).unwrap_or(());
                });
            }
            Ok(())
        }
    }

    pub fn watch<T: Target>(&mut self, target: T) -> Result<()> {
        let fd = t.as_raw_fd();
        let mut ev = libc::epoll_event {
            events: target.mask() as libc::uint32_t,
            u64: fd as libc::uint64_t,
        };
        self.pending.insert(fd, Box::new(target));
        let rv = unsafe { libc::epoll_ctl(
            self.qfd,
            libc::EPOLL_CTL_ADD,
            fd as libc::c_int,
            &mut ev as *mut libc::epoll_event)
        };
        writeln!(stderr(), "watch fd={} as {}", fd, rv).unwrap_or(());
        if rv == -1 {
            Err(Error::last_os_error())
        } else {
            Ok(())
        }
    }

    pub fn unwatch<T: Target>(&mut self, fd: RawFd) -> Option<Target> {
        None
    }
}

impl Drop for Queue {
    fn drop(&mut self) {
        if self.qfd != -1 {
            unsafe { libc::close(self.qfd); };
        }
    }
}

