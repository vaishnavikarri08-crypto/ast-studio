import { MessageSquare } from "lucide-react";

export default function CommentBadge({ comments }: { comments?: string[] }) {
  if (!comments?.length) return null;
  return (
    <span
      className="comment-badge"
      title={comments.join("\n")}
      aria-label={`${comments.length} comment${comments.length > 1 ? "s" : ""}: ${comments.join("; ")}`}
    >
      <MessageSquare size={11} />
      {comments.length}
    </span>
  );
}
