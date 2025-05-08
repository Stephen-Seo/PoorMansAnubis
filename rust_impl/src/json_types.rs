use serde::{Deserialize, Serialize};

#[derive(Debug, PartialEq, Eq, Clone, Serialize, Deserialize)]
pub struct FactorsResponse {
    pub r#type: String,
    pub id: String,
    pub factors: String,
}
