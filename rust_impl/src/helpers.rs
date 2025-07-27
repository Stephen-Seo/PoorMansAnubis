use crate::error::Error;

pub fn validate_client_response(resp: &str) -> Result<(), Error> {
    #[derive(PartialEq, Debug)]
    enum State {
        GetNum,
        GetAmt,
        GetWhitespace,
    }

    let mut state = State::GetNum;
    let mut num: u64 = 0;
    let mut max_num: u64 = 0;

    for c in resp.chars() {
        match &state {
            State::GetNum => {
                if c.is_digit(10) {
                    num = num * 10
                        + c.to_digit(10).ok_or(Error::Generic(
                            "Failed to parse digit in client response".into(),
                        ))? as u64;
                } else if c == 'x' {
                    if max_num >= num {
                        return Err(Error::Generic(
                            "Invalid client response, numbers out of order".into(),
                        ));
                    }
                    max_num = num;
                    num = 0;
                    state = State::GetAmt;
                } else {
                    return Err(Error::Generic(
                        "Invalid state parsing client response".into(),
                    ));
                }
            }
            State::GetAmt => {
                if c.is_digit(10) {
                    // Intentionally left blank.
                } else if c.is_whitespace() {
                    state = State::GetWhitespace;
                } else {
                    return Err(Error::Generic(
                        "Invalid state parsing client response".into(),
                    ));
                }
            }
            State::GetWhitespace => {
                if c.is_whitespace() {
                    // Intentionally left blank.
                } else if c.is_digit(10) {
                    state = State::GetNum;
                    num = c.to_digit(10).ok_or(Error::Generic(
                        "Failed to parse digit in client response".into(),
                    ))? as u64;
                } else {
                    return Err(Error::Generic(
                        "Invalid state parsing client response".into(),
                    ));
                }
            }
        }
    }
    if state != State::GetAmt {
        return Err(Error::Generic(
            "Invalid end state parsing client response".into(),
        ));
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_validate() {
        let mut ret = validate_client_response("1x1 2x2 3x3");
        // println!("{:?}", ret);
        assert!(ret.is_ok());
        ret = validate_client_response("2x1 1x2 3x3");
        // println!("{:?}", ret);
        assert!(!ret.is_ok());
        ret = validate_client_response("3x1 3x2 3x3");
        // println!("{:?}", ret);
        assert!(!ret.is_ok());
    }
}
